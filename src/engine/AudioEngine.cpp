#include "engine/AudioEngine.h"

#include "rec/DiskWriter.h"

#include <cmath>
#include <thread>
#include <utility>

namespace bb
{

namespace
{
    constexpr int midiBufferBytes = 1024 * 16; // espaço reservado: ~1024 eventos por bloco sem alocar
}

AudioEngine::AudioEngine()
{
    midiBuffer.ensureSize (midiBufferBytes);
    chunkMidi.ensureSize (midiBufferBytes);
}

AudioEngine::~AudioEngine()
{
    // O dono deve ter removido este callback do AudioDeviceManager antes.
    if (plugin != nullptr)
        plugin->releaseResources();
}

// ---------------------------------------------------------------------------
// Thread de mensagens
// ---------------------------------------------------------------------------

std::unique_ptr<juce::AudioPluginInstance> AudioEngine::setPlugin (std::unique_ptr<juce::AudioPluginInstance> newPlugin)
{
    const auto sr = sampleRate.load();
    const auto bs = blockSize.load();

    // Prepara antes de publicar: a thread de áudio ainda não enxerga o plugin novo.
    if (newPlugin != nullptr && deviceRunning.load() && sr > 0.0 && bs > 0)
        preparePlugin (*newPlugin, sr, bs);

    std::unique_ptr<juce::AudioPluginInstance> old;

    {
        // A thread de áudio só faz tryLock: segurar aqui nunca a faz esperar.
        const juce::SpinLock::ScopedLockType lock (pluginLock);
        old = std::move (plugin);
        plugin = std::move (newPlugin);
        allocateBuffers (juce::jmax (bs, 512));
    }

    if (old != nullptr)
        old->releaseResources();

    return old;
}

void AudioEngine::panic()
{
    panicRequested.store (true);
}

void AudioEngine::playNote (int note, int velocity, bool on) noexcept
{
    MidiEvent e;
    e.size = 3;
    e.data[0] = on ? 0x99 : 0x89; // canal 10
    e.data[1] = (std::uint8_t) juce::jlimit (0, 127, note);
    e.data[2] = (std::uint8_t) (on ? juce::jlimit (1, 127, velocity) : 0);
    e.timeSec = juce::Time::getMillisecondCounterHiRes() * 0.001;
    pushUiMidi (e);
}

void AudioEngine::preparePlugin (juce::AudioPluginInstance& p, double sr, int bs)
{
    p.setNonRealtime (false);
    p.setRateAndBufferSizeDetails (sr, bs);
    p.prepareToPlay (sr, bs);
}

void AudioEngine::allocateBuffers (int bs)
{
    int channels = 2;

    if (plugin != nullptr)
        channels = juce::jmax (2, plugin->getTotalNumInputChannels(), plugin->getTotalNumOutputChannels());

    pluginBuffer.setSize (channels, bs, false, true, false);
    pluginBufferChannels = channels;
}

// ---------------------------------------------------------------------------
// Ciclo de vida do dispositivo (chamado fora da thread de áudio, com o stream parado)
// ---------------------------------------------------------------------------

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    const auto sr = device->getCurrentSampleRate();
    const auto bs = device->getCurrentBufferSizeSamples();

    {
        const juce::SpinLock::ScopedLockType lock (pluginLock);
        sampleRate.store (sr);
        blockSize.store (bs);
        allocateBuffers (bs);

        if (plugin != nullptr)
            preparePlugin (*plugin, sr, bs);
    }

    midiBuffer.clear();
    chunkMidi.clear();

    // Chrome: o resampler e o buffer são alocados aqui (o stream de áudio ainda não começou).
    chromeResampler.prepare (sr, juce::jmax (bs, 512));
    chromeBuffer.setSize (2, juce::jmax (bs, 512), false, true, false);

    // Mixer: barramento estéreo com folga (o ASIO sempre entrega o mesmo tamanho de bloco).
    mixBus.setSize (2, juce::jmax (bs, 4096), false, true, false);
    numDeviceInputs.store (device->getActiveInputChannels().countNumberOfSetBits());

    deviceRunning.store (true);
}

void AudioEngine::audioDeviceStopped()
{
    deviceRunning.store (false);

    // O stream parou: a thread de áudio não roda mais, então dá para encerrar o transporte daqui.
    const int pending = pendingRequest.exchange (reqNone);
    if (getTransportState() != TransportState::idle || pending == reqRecord || pending == reqPlay)
        endTransport (false);

    const juce::SpinLock::ScopedLockType lock (pluginLock);
    if (plugin != nullptr)
        plugin->releaseResources();
}

// ---------------------------------------------------------------------------
// Thread de áudio
// ---------------------------------------------------------------------------

void AudioEngine::collectMidi (int numSamples, double blockStartSec)
{
    const auto sr = sampleRate.load (std::memory_order_relaxed);
    const bool steady = midiTiming.load (std::memory_order_relaxed) == (int) MidiTiming::steady;

    if (panicRequested.exchange (false))
    {
        for (int ch = 1; ch <= 16; ++ch)
        {
            midiBuffer.addEvent (juce::MidiMessage::allSoundOff (ch), 0);
            midiBuffer.addEvent (juce::MidiMessage::allNotesOff (ch), 0);
        }
    }

    const auto drain = [&] (auto& queue)
    {
        MidiEvent e;
        while (queue.pop (e))
        {
            int offset = 0;

            if (steady)
            {
                // Atraso fixo de 1 bloco: o evento cai na mesma posição relativa em que chegou,
                // então o ritmo fica exato (sem a oscilação de até 1 buffer do modo rápido).
                offset = (int) std::floor ((e.timeSec - blockStartSec) * sr);
                offset = juce::jlimit (0, numSamples - 1, offset);
            }

            midiBuffer.addEvent (e.data, (int) e.size, offset);
        }
    };

    drain (midiQueue);
    drain (uiMidiQueue);
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                                    float* const* outputs, int numOutputs,
                                                    int numSamplesIn, const juce::AudioIODeviceCallbackContext&)
{
    for (int ch = 0; ch < numOutputs; ++ch)
        if (outputs[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputs[ch], numSamplesIn);

    const auto sr = sampleRate.load (std::memory_order_relaxed);
    const int numSamples = juce::jmin (numSamplesIn, mixBus.getNumSamples());
    if (sr <= 0.0 || numSamples <= 0)
        return;

    applyTransportRequest();
    computeTrackGains();
    mixBus.clear (0, numSamples);

    // Início "virtual" deste bloco = agora menos a duração de um bloco (usado no modo estável).
    const double nowSec = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const double blockStartSec = nowSec - numSamples / sr;

    midiBuffer.clear();
    collectMidi (numSamples, blockStartSec);
    addPlaybackEvents (numSamples);

    {
        const juce::SpinLock::ScopedTryLockType lock (pluginLock);
        const int maxChunk = pluginBuffer.getNumSamples();

        if (! lock.isLocked() || plugin == nullptr || maxChunk <= 0)
        {
            if (! lock.isLocked())
                skippedBlocks.fetch_add (1, std::memory_order_relaxed);

            // Eventos deste bloco são descartados (bateria: notas curtas). Na gravação entra silêncio
            // do mesmo tamanho, para o WAV continuar alinhado com o MIDI.
            recordDrumSilence (numSamples);
        }
        else
        {
            for (int start = 0; start < numSamples; start += maxChunk)
                renderChunk (start, juce::jmin (maxChunk, numSamples - start));
        }
    }

    processChrome (numSamples);
    processInputs (inputs, numInputs, numSamples);
    processMaster (outputs, numOutputs, numSamples);

    sampleCounter += (std::uint64_t) numSamples;
    if (getTransportState() != TransportState::idle)
        transportPosition.store (sampleCounter - transportStart, std::memory_order_relaxed);
}

void AudioEngine::storePeak (std::atomic<float>& target, float value) noexcept
{
    float current = target.load (std::memory_order_relaxed);
    while (value > current && ! target.compare_exchange_weak (current, value, std::memory_order_relaxed)) {}
}

void AudioEngine::computeTrackGains() noexcept
{
    bool anySolo = false;
    for (auto& t : tracks)
        anySolo = anySolo || t.solo.load (std::memory_order_relaxed);

    for (int i = 0; i < numTracks; ++i)
    {
        const auto& t = tracks[i];
        const bool audible = ! t.mute.load (std::memory_order_relaxed)
                          && (! anySolo || t.solo.load (std::memory_order_relaxed));
        blockGains[i] = audible ? t.gain.load (std::memory_order_relaxed) : 0.0f;
    }
}

void AudioEngine::renderChunk (int start, int len)
{
    auto& p = *plugin;

    // Mesmo lock que o AudioProcessorPlayer usa, mas sem esperar.
    const juce::ScopedTryLock processorLock (p.getCallbackLock());
    if (! processorLock.isLocked() || p.isSuspended())
    {
        recordDrumSilence (len);
        return;
    }

    chunkMidi.clear();
    chunkMidi.addEvents (midiBuffer, start, len, -start);
    captureRecordedMidi (start); // antes do processBlock: o plugin pode reescrever o buffer de MIDI

    pluginBuffer.setSize (pluginBufferChannels, len, false, false, true); // sem realocar
    pluginBuffer.clear();
    p.processBlock (pluginBuffer, chunkMidi);

    const float gain = blockGains[trackDrums];
    const int pluginOuts = juce::jmin (p.getTotalNumOutputChannels(), pluginBufferChannels);

    const float* left  = pluginOuts > 0 ? pluginBuffer.getReadPointer (0) : nullptr;
    const float* right = pluginOuts > 1 ? pluginBuffer.getReadPointer (1) : left;

    if (left == nullptr)
    {
        recordDrumSilence (len);
        return;
    }

    recordDrumAudio (left, right, len); // saída crua do plugin (o volume só afeta o que você ouve)

    juce::FloatVectorOperations::addWithMultiply (mixBus.getWritePointer (0, start), left, gain, len);
    juce::FloatVectorOperations::addWithMultiply (mixBus.getWritePointer (1, start), right, gain, len);

    const auto rangeL = juce::FloatVectorOperations::findMinAndMax (left, len);
    const auto rangeR = juce::FloatVectorOperations::findMinAndMax (right, len);
    const float peak = juce::jmax (juce::jmax (std::abs (rangeL.getStart()), std::abs (rangeL.getEnd())),
                                   juce::jmax (std::abs (rangeR.getStart()), std::abs (rangeR.getEnd())));
    storePeak (tracks[trackDrums].peak, peak * gain);
}

// ---------------------------------------------------------------------------
// Transporte: pedidos (thread de mensagens)
// ---------------------------------------------------------------------------

bool AudioEngine::requestRecord (const RecordTargets& targets) noexcept
{
    if (getTransportState() != TransportState::idle || pendingRequest.load() != reqNone)
        return false;

    requestedTargets = targets;
    requestedTargets.pluginLatency = juce::jmax (0, targets.pluginLatency);
    requestedTargets.inputCompensation = juce::jmax (0, targets.inputCompensation);
    resourcesReleased.store (false);
    pendingRequest.store (reqRecord, std::memory_order_release);
    return true;
}

bool AudioEngine::requestPlay (const TimedMidi* events, int numEvents) noexcept
{
    if (getTransportState() != TransportState::idle || pendingRequest.load() != reqNone || numEvents <= 0)
        return false;

    requestedEvents = events;
    requestedNumEvents = numEvents;
    resourcesReleased.store (false);
    pendingRequest.store (reqPlay, std::memory_order_release);
    return true;
}

void AudioEngine::requestStop() noexcept
{
    const int previous = pendingRequest.exchange (reqStop, std::memory_order_acq_rel);

    // Gravar/tocar pedido mas ainda não aplicado: nunca começou, os recursos já podem ser liberados.
    if (previous == reqRecord || previous == reqPlay)
        resourcesReleased.store (true, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Transporte: thread de áudio
// ---------------------------------------------------------------------------

void AudioEngine::applyTransportRequest()
{
    const int request = pendingRequest.exchange (reqNone, std::memory_order_acquire);
    if (request == reqNone)
        return;

    const auto state = getTransportState();

    if (request == reqStop)
    {
        if (state != TransportState::idle)
            endTransport (state == TransportState::playing);
        return;
    }

    if (state != TransportState::idle)
    {
        resourcesReleased.store (true, std::memory_order_release); // pedido recusado: devolve os recursos
        return;
    }

    transportStart = sampleCounter;
    transportPosition.store (0, std::memory_order_relaxed);

    if (request == reqRecord)
    {
        const auto& t = requestedTargets;
        activeWriter = t.drum;
        activeChromeWriter = t.chrome;
        audioSkipRemaining = t.pluginLatency;
        chromeSkipRemaining = t.pluginLatency;
        for (int k = 0; k < numInputTracks; ++k)
        {
            activeInputWriters[k] = t.inputs[k];
            inputSkipRemaining[k] = t.inputCompensation + t.pluginLatency;
        }
        transportState.store ((int) TransportState::recording, std::memory_order_release);
    }
    else if (request == reqPlay)
    {
        playEvents = requestedEvents;
        playNumEvents = requestedNumEvents;
        playIndex = 0;
        transportState.store ((int) TransportState::playing, std::memory_order_release);
    }
}

void AudioEngine::endTransport (bool sendNotesOff)
{
    activeWriter = nullptr;
    activeChromeWriter = nullptr;
    for (auto& w : activeInputWriters)
        w = nullptr;
    playEvents = nullptr;
    playNumEvents = playIndex = 0;
    notesOffRequested = notesOffRequested || sendNotesOff;
    transportState.store ((int) TransportState::idle, std::memory_order_release);
    resourcesReleased.store (true, std::memory_order_release);
}

void AudioEngine::addPlaybackEvents (int numSamples)
{
    if (notesOffRequested)
    {
        notesOffRequested = false;
        for (int ch = 1; ch <= 16; ++ch)
            midiBuffer.addEvent (juce::MidiMessage::allNotesOff (ch), 0);
    }

    if (getTransportState() != TransportState::playing || playEvents == nullptr)
        return;

    const std::uint64_t blockStart = sampleCounter - transportStart;
    const std::uint64_t blockEnd = blockStart + (std::uint64_t) numSamples;

    while (playIndex < playNumEvents && playEvents[playIndex].samplePos < blockEnd)
    {
        const auto& e = playEvents[playIndex++];
        const int offset = e.samplePos > blockStart ? (int) (e.samplePos - blockStart) : 0;
        midiBuffer.addEvent (e.data, (int) e.size, offset);
    }

    if (playIndex >= playNumEvents)
        endTransport (false); // acabou o take: os pratos continuam soando naturalmente
}

void AudioEngine::captureRecordedMidi (int chunkStart)
{
    if (getTransportState() != TransportState::recording)
        return;

    const std::uint64_t base = (sampleCounter - transportStart) + (std::uint64_t) chunkStart;

    for (const auto metadata : chunkMidi)
    {
        if (metadata.numBytes < 1 || metadata.numBytes > 3)
            continue;

        TimedMidi e;
        e.samplePos = base + (std::uint64_t) metadata.samplePosition;
        e.size = (std::uint8_t) metadata.numBytes;
        for (int i = 0; i < metadata.numBytes; ++i)
            e.data[i] = metadata.data[i];

        if (! recordedMidi.push (e))
            recordedMidiDropped.fetch_add (1, std::memory_order_relaxed);
    }
}

void AudioEngine::recordDrumAudio (const float* left, const float* right, int numSamples) noexcept
{
    if (getTransportState() != TransportState::recording || activeWriter == nullptr)
        return;

    if (audioSkipRemaining > 0)
    {
        const int skip = juce::jmin (audioSkipRemaining, numSamples);
        audioSkipRemaining -= skip;
        left += skip;
        right += skip;
        numSamples -= skip;
    }

    const float* channels[] = { left, right };
    activeWriter->write (channels, 2, numSamples);
}

void AudioEngine::setChromeStream (LoopbackStream* stream) noexcept
{
    // seq_cst nos dois lados (padrão de Dekker): depois deste laço, a thread de áudio ou já viu o
    // ponteiro novo, ou terminou de usar o antigo.
    chromeStream.store (stream);
    while (chromeInUse.load())
        std::this_thread::yield();
    chromeResampler.requestReset();
}

void AudioEngine::processChrome (int numSamples) noexcept
{
    const int maxChunk = chromeBuffer.getNumSamples();
    if (maxChunk <= 0)
        return;

    const bool recording = getTransportState() == TransportState::recording && activeChromeWriter != nullptr;
    const float gain = blockGains[trackChrome];

    for (int start = 0; start < numSamples; start += maxChunk)
    {
        const int len = juce::jmin (maxChunk, numSamples - start);
        float* l = chromeBuffer.getWritePointer (0);
        float* r = chromeBuffer.getWritePointer (1);

        chromeInUse.store (true);
        chromeResampler.process (chromeStream.load(), l, r, len); // sem stream: silêncio
        chromeInUse.store (false);

        if (recording)
        {
            // Sempre escreve (mesmo silêncio): o chrome.wav fica com o mesmo tamanho e início da bateria.
            const int skip = juce::jmin (chromeSkipRemaining, len);
            chromeSkipRemaining -= skip;
            const float* channels[] = { l + skip, r + skip };
            activeChromeWriter->write (channels, 2, len - skip);
        }

        juce::FloatVectorOperations::addWithMultiply (mixBus.getWritePointer (0, start), l, gain, len);
        juce::FloatVectorOperations::addWithMultiply (mixBus.getWritePointer (1, start), r, gain, len);

        const auto rangeL = juce::FloatVectorOperations::findMinAndMax (l, len);
        const auto rangeR = juce::FloatVectorOperations::findMinAndMax (r, len);
        storePeak (tracks[trackChrome].peak,
                   gain * juce::jmax (juce::jmax (std::abs (rangeL.getStart()), std::abs (rangeL.getEnd())),
                                      juce::jmax (std::abs (rangeR.getStart()), std::abs (rangeR.getEnd()))));
    }
}

void AudioEngine::processInputs (const float* const* inputs, int numInputs, int numSamples) noexcept
{
    const bool recording = getTransportState() == TransportState::recording;

    for (int k = 0; k < numInputTracks; ++k)
    {
        auto& t = tracks[trackInput1 + k];
        const int ch = t.channel.load (std::memory_order_relaxed);
        const float* in = (inputs != nullptr && ch < numInputs) ? inputs[ch] : nullptr;

        if (in != nullptr)
        {
            const auto range = juce::FloatVectorOperations::findMinAndMax (in, numSamples);
            const float peak = juce::jmax (std::abs (range.getStart()), std::abs (range.getEnd()));
            storePeak (t.peak, peak);                 // antes do fader: nível que vai para a gravação
            if (peak >= 0.999f)
                t.clip.store (true, std::memory_order_relaxed);
        }

        if (recording && activeInputWriters[k] != nullptr)
        {
            // Adianta a entrada: descarta as primeiras (latência de entrada + saída) amostras.
            const int skip = juce::jmin (inputSkipRemaining[k], numSamples);
            inputSkipRemaining[k] -= skip;
            if (in != nullptr)
            {
                const float* channels[] = { in + skip };
                activeInputWriters[k]->write (channels, 1, numSamples - skip);
            }
            else
            {
                activeInputWriters[k]->writeSilence (numSamples - skip); // sem o canal: mantém o alinhamento
            }
        }

        const float gain = blockGains[trackInput1 + k];
        if (in != nullptr && gain > 0.0f && t.monitor.load (std::memory_order_relaxed))
        {
            juce::FloatVectorOperations::addWithMultiply (mixBus.getWritePointer (0), in, gain, numSamples);
            juce::FloatVectorOperations::addWithMultiply (mixBus.getWritePointer (1), in, gain, numSamples);
        }
    }
}

void AudioEngine::processMaster (float* const* outputs, int numOutputs, int numSamples) noexcept
{
    const float gain = masterGain.load (std::memory_order_relaxed);
    mixBus.applyGain (0, numSamples, gain);

    const int pair = outputPair.load (std::memory_order_relaxed);
    float* outL = (pair < numOutputs)     ? outputs[pair]     : nullptr;
    float* outR = (pair + 1 < numOutputs) ? outputs[pair + 1] : nullptr;
    if (outL != nullptr) juce::FloatVectorOperations::copy (outL, mixBus.getReadPointer (0), numSamples);
    if (outR != nullptr) juce::FloatVectorOperations::copy (outR, mixBus.getReadPointer (1), numSamples);

    const float pl = mixBus.getMagnitude (0, 0, numSamples);
    const float pr = mixBus.getMagnitude (1, 0, numSamples);
    storePeak (masterPeakL, pl);
    storePeak (masterPeakR, pr);
    if (pl >= 1.0f || pr >= 1.0f)
        masterClip.store (true, std::memory_order_relaxed);
}

void AudioEngine::recordDrumSilence (int numSamples) noexcept
{
    if (getTransportState() != TransportState::recording || activeWriter == nullptr)
        return;

    if (audioSkipRemaining > 0)
    {
        const int skip = juce::jmin (audioSkipRemaining, numSamples);
        audioSkipRemaining -= skip;
        numSamples -= skip;
    }

    activeWriter->writeSilence (numSamples);
}

} // namespace bb
