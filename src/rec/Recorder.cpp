#include "rec/Recorder.h"

namespace bb
{

namespace
{
    bool isNoteOnBytes (const std::uint8_t* d, int size) { return size >= 3 && (d[0] & 0xF0) == 0x90 && d[2] > 0; }
}

Recorder::Recorder (AudioEngine& e) : engine (e)
{
    recorded.reserve (1 << 16);
}

Recorder::~Recorder()
{
    // O dono deve parar o áudio antes. Se ainda houver gravação, fecha o arquivo do jeito possível.
    if (state == State::recording || state == State::finishing)
    {
        engine.requestStop();
        drainRecordedMidi();
    }
    if (drumWriter != nullptr)
        drumWriter->finish();
    if (chromeWriter != nullptr)
        chromeWriter->finish();
    for (auto& w : inputWriters)
        if (w != nullptr)
            w->finish();
}

void Recorder::finishAllWriters()
{
    for (auto* w : { &drumWriter, &chromeWriter, &inputWriters[0], &inputWriters[1] })
        if (*w != nullptr)
        {
            (*w)->finish();
            w->reset();
        }
}

juce::File Recorder::defaultRecordingsDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Backbeat");
}

void Recorder::setState (State s)
{
    if (state == s)
        return;
    state = s;
    if (onStateChanged)
        onStateChanged();
}

double Recorder::getPositionSeconds() const
{
    const double sr = engine.getSampleRate();
    return sr > 0.0 ? (double) engine.getTransportPosition() / sr : 0.0;
}

// ---------------------------------------------------------------------------
// Gravar
// ---------------------------------------------------------------------------

bool Recorder::startRecording (const juce::File& recordingsDir, const Metadata& meta, juce::String& error)
{
    if (state != State::idle)
    {
        error = L"Pare antes de gravar de novo.";
        return false;
    }

    const double sr = engine.getSampleRate();
    if (sr <= 0.0)
    {
        error = L"O dispositivo de áudio não está aberto.";
        return false;
    }

    startedAt = juce::Time::getCurrentTime();
    auto folder = recordingsDir.getChildFile (startedAt.formatted ("%Y-%m-%d %H-%M-%S"));
    for (int n = 2; folder.exists(); ++n)
        folder = recordingsDir.getChildFile (startedAt.formatted ("%Y-%m-%d %H-%M-%S") + " (" + juce::String (n) + ")");

    if (! folder.createDirectory())
    {
        error = L"Não foi possível criar a pasta " + folder.getFullPathName();
        return false;
    }

    // 10 s de folga no FIFO: o disco pode engasgar sem perder áudio.
    drumWriter = DiskWriter::createWav (folder.getChildFile ("bateria.wav"), sr, 2, 24, 10.0, error);
    if (drumWriter == nullptr)
        return false;

    if (meta.recordChrome)
    {
        chromeWriter = DiskWriter::createWav (folder.getChildFile ("chrome.wav"), sr, 2, 24, 10.0, error);
        if (chromeWriter == nullptr)
        {
            finishAllWriters();
            return false;
        }
    }

    for (int k = 0; k < numInputTracks; ++k)
    {
        if (! meta.recordInput[k])
            continue;

        inputWriters[k] = DiskWriter::createWav (folder.getChildFile ("entrada-" + juce::String (k + 1) + ".wav"),
                                                 sr, 1, 24, 10.0, error);
        if (inputWriters[k] == nullptr)
        {
            finishAllWriters();
            return false;
        }
    }
    chromeUnderrunsAtStart = engine.getChromeResampler().getUnderruns();

    TimedMidi leftover;
    while (engine.popRecordedMidi (leftover)) {} // descarta restos
    recorded.clear();
    liveNoteCount = 0;
    midiDroppedAtStart = engine.getRecordedMidiDropped();
    currentFolder = folder;
    currentMeta = meta;
    recordSampleRate = sr;

    RecordTargets targets;
    targets.drum = drumWriter.get();
    targets.chrome = chromeWriter.get();
    for (int k = 0; k < numInputTracks; ++k)
        targets.inputs[k] = inputWriters[k].get();
    targets.pluginLatency = meta.pluginLatencySamples;
    targets.inputCompensation = meta.inputCompensationSamples;
    targets.countInBeats = juce::jmax (0, meta.countInBars) * juce::jmax (1, meta.beatsPerBar);

    if (! engine.requestRecord (targets))
    {
        finishAllWriters();
        folder.deleteRecursively();
        error = L"O transporte está ocupado.";
        return false;
    }

    setState (State::recording);
    message (targets.countInBeats > 0 ? juce::String (L"Contagem... a gravação começa no tempo 1.")
                                      : L"Gravando em " + folder.getFileName());
    return true;
}

void Recorder::drainRecordedMidi()
{
    TimedMidi e;
    while (engine.popRecordedMidi (e))
    {
        recorded.push_back (e);
        if (isNoteOnBytes (e.data, e.size))
            ++liveNoteCount;
    }
}

void Recorder::finishRecording()
{
    drainRecordedMidi();

    const auto duration = drumWriter != nullptr ? (double) drumWriter->getWrittenSamples() / recordSampleRate : 0.0;

    // Parou na contagem (ou antes do primeiro bloco): não sobra take vazio.
    if (duration <= 0.0 && recorded.empty())
    {
        finishAllWriters();
        currentFolder.deleteRecursively();
        message (L"Gravação cancelada antes de começar.");
        return;
    }
    std::uint64_t droppedAudio = 0;

    if (drumWriter != nullptr)
    {
        drumWriter->finish();
        droppedAudio = drumWriter->getDroppedSamples();
        drumWriter.reset();
    }

    const bool hadChrome = chromeWriter != nullptr;
    if (hadChrome)
    {
        chromeWriter->finish();
        droppedAudio += chromeWriter->getDroppedSamples();
        chromeWriter.reset();
    }
    const auto chromeGaps = engine.getChromeResampler().getUnderruns() - chromeUnderrunsAtStart;

    // Entradas: foram gravadas adiantadas; completa o fim com o mesmo tanto de silêncio para terem o
    // mesmo tamanho da bateria. A thread de áudio já largou os writers: aqui a escrita é só desta thread.
    bool hadInput[numInputTracks] {};
    for (int k = 0; k < numInputTracks; ++k)
    {
        if (inputWriters[k] == nullptr)
            continue;
        hadInput[k] = true;
        for (int left = currentMeta.inputCompensationSamples; left > 0; left -= 4096)
            inputWriters[k]->writeSilence (juce::jmin (4096, left));
        inputWriters[k]->finish();
        droppedAudio += inputWriters[k]->getDroppedSamples();
        inputWriters[k].reset();
    }

    auto take = std::make_unique<Take>();
    take->folder = currentFolder;
    take->noteCount = liveNoteCount;

    for (const auto& r : recorded)
    {
        TakeEvent e;
        e.timeSec = (double) r.samplePos / recordSampleRate;
        e.size = r.size;
        for (int i = 0; i < 3; ++i)
            e.data[i] = r.data[i];
        take->events.push_back (e);
    }
    take->durationSec = juce::jmax (duration, take->events.empty() ? 0.0 : take->events.back().timeSec);

    const bool midiOk = takefiles::writeMidiFile (currentFolder.getChildFile ("bateria.mid"), take->events,
                                                  currentMeta.bpm, "Bateria", currentMeta.beatsPerBar);

    const auto droppedMidi = engine.getRecordedMidiDropped() - midiDroppedAtStart;

    // sessao.json
    auto* root = new juce::DynamicObject();
    root->setProperty ("versao", 1);
    root->setProperty ("inicio", startedAt.toISO8601 (true));
    root->setProperty ("duracaoSegundos", take->durationSec);
    root->setProperty ("taxaAmostragem", recordSampleRate);
    root->setProperty ("bpm", currentMeta.bpm);
    root->setProperty ("compasso", juce::String (currentMeta.beatsPerBar) + "/4");
    root->setProperty ("contagemCompassos", currentMeta.countInBars);
    root->setProperty ("perfil", currentMeta.profile);
    root->setProperty ("kit", currentMeta.kit);
    root->setProperty ("plugin", currentMeta.plugin);
    root->setProperty ("tempoDasNotas", currentMeta.midiTiming);
    root->setProperty ("notas", take->noteCount);

    juce::Array<juce::var> tracks;
    {
        auto* t = new juce::DynamicObject();
        t->setProperty ("tipo", "midi");
        t->setProperty ("nome", "Bateria");
        t->setProperty ("arquivo", "bateria.mid");
        tracks.add (juce::var (t));
    }
    {
        auto* t = new juce::DynamicObject();
        t->setProperty ("tipo", "audio");
        t->setProperty ("nome", "Bateria (" + (currentMeta.plugin.isNotEmpty() ? currentMeta.plugin : juce::String ("plugin")) + ")");
        t->setProperty ("arquivo", "bateria.wav");
        t->setProperty ("canais", 2);
        t->setProperty ("bits", 24);
        t->setProperty ("latenciaPluginCompensadaAmostras", currentMeta.pluginLatencySamples);
        tracks.add (juce::var (t));
    }
    if (hadChrome)
    {
        auto* t = new juce::DynamicObject();
        t->setProperty ("tipo", "audio");
        t->setProperty ("nome", "Chrome");
        t->setProperty ("arquivo", "chrome.wav");
        t->setProperty ("canais", 2);
        t->setProperty ("bits", 24);
        t->setProperty ("processo", currentMeta.chromeProcess);
        t->setProperty ("diferencaDeRelogioPpm", engine.getChromeResampler().getRatioPpm());
        t->setProperty ("atrasoDoBufferMs", engine.getChromeResampler().getTargetMs());
        t->setProperty ("faltasDeAudio", (int) chromeGaps);
        tracks.add (juce::var (t));
    }
    for (int k = 0; k < numInputTracks; ++k)
    {
        if (! hadInput[k])
            continue;
        auto* t = new juce::DynamicObject();
        t->setProperty ("tipo", "audio");
        t->setProperty ("nome", currentMeta.inputName[k].isNotEmpty() ? currentMeta.inputName[k] : "Entrada " + juce::String (k + 1));
        t->setProperty ("arquivo", "entrada-" + juce::String (k + 1) + ".wav");
        t->setProperty ("canais", 1);
        t->setProperty ("bits", 24);
        t->setProperty ("compensacaoDeLatenciaAmostras", currentMeta.inputCompensationSamples);
        t->setProperty ("compensacaoDeLatenciaMs", currentMeta.inputCompensationSamples * 1000.0 / recordSampleRate);
        tracks.add (juce::var (t));
    }
    root->setProperty ("faixas", tracks);

    auto* losses = new juce::DynamicObject();
    losses->setProperty ("eventosMidi", (int) droppedMidi);
    losses->setProperty ("amostrasDeAudio", (juce::int64) droppedAudio);
    root->setProperty ("perdas", juce::var (losses));

    currentFolder.getChildFile ("sessao.json").replaceWithText (juce::JSON::toString (juce::var (root)), false, false, "\n");

    lastTake = std::move (take);
    recorded.clear();

    if (droppedAudio > 0 || droppedMidi > 0)
        message (L"Gravação salva, mas com perdas (áudio: " + juce::String ((juce::int64) droppedAudio)
                     + " amostras, MIDI: " + juce::String ((int) droppedMidi) + L" eventos). O disco está lento?", true);
    else if (! midiOk)
        message (L"Áudio salvo, mas não foi possível gravar o bateria.mid.", true);
    else
        message (L"Gravação salva: " + currentFolder.getFileName() + " (" + juce::String (lastTake->noteCount)
                 + L" notas, " + juce::String (lastTake->durationSec, 1).replaceCharacter ('.', ',') + " s)");
}

// ---------------------------------------------------------------------------
// Tocar
// ---------------------------------------------------------------------------

bool Recorder::startPlayback (juce::String& error)
{
    if (state != State::idle)
    {
        error = L"Pare antes de tocar.";
        return false;
    }

    if (lastTake == nullptr)
    {
        error = L"Nada gravado ainda.";
        return false;
    }

    if (lastTake->events.empty())
    {
        error = L"O último take não tem notas (o áudio está em " + lastTake->folder.getFileName() + ").";
        return false;
    }

    const double sr = engine.getSampleRate();
    if (sr <= 0.0)
    {
        error = L"O dispositivo de áudio não está aberto.";
        return false;
    }

    auto events = lastTake->events;
    takefiles::addMissingNoteOffs (events);

    playback.clear();
    playback.reserve (events.size());
    for (const auto& e : events)
    {
        TimedMidi t;
        t.samplePos = (std::uint64_t) std::llround (juce::jmax (0.0, e.timeSec) * sr);
        t.size = e.size;
        for (int i = 0; i < 3; ++i)
            t.data[i] = e.data[i];
        playback.push_back (t);
    }

    if (! engine.requestPlay (playback.data(), (int) playback.size()))
    {
        error = L"O transporte está ocupado.";
        return false;
    }

    setState (State::playing);
    message (L"Tocando " + lastTake->folder.getFileName());
    return true;
}

// ---------------------------------------------------------------------------
// Parar e acompanhar
// ---------------------------------------------------------------------------

void Recorder::stop()
{
    if (state != State::recording && state != State::playing)
        return;

    finishingFrom = state;
    engine.requestStop();
    setState (State::finishing);
}

void Recorder::poll()
{
    if (state == State::recording)
    {
        drainRecordedMidi();

        // A thread de áudio parou sozinha (ex.: dispositivo fechou)?
        if (engine.takeResourcesReleased())
        {
            finishingFrom = State::recording;
            setState (State::finishing);
        }
    }
    else if (state == State::playing)
    {
        if (engine.takeResourcesReleased()) // o take acabou
        {
            playback.clear();
            setState (State::idle);
            message (L"Fim do take.");
        }
        return;
    }

    if (state != State::finishing)
        return;

    drainRecordedMidi();

    if (engine.getTransportState() != TransportState::idle && ! engine.takeResourcesReleased())
        return; // a thread de áudio ainda não largou os recursos

    engine.takeResourcesReleased(); // zera o aviso, se ainda estiver marcado

    if (finishingFrom == State::recording)
        finishRecording();
    else
        playback.clear();

    setState (State::idle);
}

bool Recorder::loadLatestTake (const juce::File& recordingsDir)
{
    lastTake.reset();
    auto folders = recordingsDir.findChildFiles (juce::File::findDirectories, false);
    folders.sort(); // nomes AAAA-MM-DD HH-MM-SS ordenam por data

    for (int i = folders.size(); --i >= 0;)
    {
        // Take = pasta com notas no bateria.mid ou com o bateria.wav (um take só de guitarra também
        // pode ser exportado, mesmo sem nenhuma nota).
        const auto midiFile = folders[i].getChildFile ("bateria.mid");
        auto events = midiFile.existsAsFile() ? takefiles::readMidiFile (midiFile) : std::vector<TakeEvent>();
        if (events.empty() && ! folders[i].getChildFile ("bateria.wav").existsAsFile())
            continue;

        auto take = std::make_unique<Take>();
        take->folder = folders[i];
        take->events = std::move (events);
        take->durationSec = take->events.empty() ? 0.0 : take->events.back().timeSec;
        const auto session = juce::JSON::parse (folders[i].getChildFile ("sessao.json"));
        if (session.hasProperty ("duracaoSegundos"))
            take->durationSec = juce::jmax (take->durationSec, (double) session["duracaoSegundos"]);
        for (const auto& e : take->events)
            if (isNoteOnBytes (e.data, e.size))
                ++take->noteCount;

        lastTake = std::move (take);
        return true;
    }
    return false;
}

} // namespace bb
