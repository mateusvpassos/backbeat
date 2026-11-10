// Testes da F3: gravação sincronizada (MIDI x áudio), reprodução e arquivos do take.
// Usa um dispositivo falso e um plugin falso que solta um impulso na amostra exata de cada nota.

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "engine/AudioEngine.h"
#include "rec/DiskWriter.h"
#include "rec/TakeFiles.h"
#include "capture/LoopbackStream.h"

#include <vector>

namespace bb
{

namespace
{
    constexpr double testRate = 44100.0;
    constexpr int testBlock = 256;

    // Dispositivo de áudio que não toca nada: só informa taxa e buffer ao AudioEngine.
    class FakeDevice final : public juce::AudioIODevice
    {
    public:
        FakeDevice() : juce::AudioIODevice ("Falso", "Teste") {}
        juce::StringArray getOutputChannelNames() override { return { "1", "2" }; }
        juce::StringArray getInputChannelNames() override { return {}; }
        juce::Array<double> getAvailableSampleRates() override { return { testRate }; }
        juce::Array<int> getAvailableBufferSizes() override { return { testBlock }; }
        int getDefaultBufferSize() override { return testBlock; }
        juce::String open (const juce::BigInteger&, const juce::BigInteger&, double, int) override { return {}; }
        void close() override {}
        bool isOpen() override { return true; }
        void start (juce::AudioIODeviceCallback*) override {}
        void stop() override {}
        bool isPlaying() override { return true; }
        juce::String getLastError() override { return {}; }
        int getCurrentBufferSizeSamples() override { return testBlock; }
        double getCurrentSampleRate() override { return testRate; }
        int getCurrentBitDepth() override { return 32; }
        juce::BigInteger getActiveOutputChannels() const override { return 3; }
        juce::BigInteger getActiveInputChannels() const override { return 0; }
        int getOutputLatencyInSamples() override { return 0; }
        int getInputLatencyInSamples() override { return 0; }
    };

    // Plugin que, para cada note-on, escreve 1.0 na amostra da nota + `latency` (simula latência própria).
    class ImpulsePlugin final : public juce::AudioPluginInstance
    {
    public:
        explicit ImpulsePlugin (int latencySamples)
            : juce::AudioPluginInstance (BusesProperties().withOutput ("Saida", juce::AudioChannelSet::stereo(), true)),
              latency (latencySamples)
        {
            setLatencySamples (latency);
        }

        void fillInPluginDescription (juce::PluginDescription& d) const override { d.name = "Impulso"; d.isInstrument = true; }
        const juce::String getName() const override { return "Impulso"; }
        void prepareToPlay (double, int) override { pending.clear(); position = 0; }
        void releaseResources() override {}

        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
        {
            const int n = buffer.getNumSamples();
            for (const auto m : midi)
            {
                const auto msg = m.getMessage();
                if (msg.isNoteOn())
                    pending.push_back (position + m.samplePosition + latency);
            }

            for (auto it = pending.begin(); it != pending.end();)
            {
                if (*it >= position && *it < position + n)
                {
                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                        buffer.setSample (ch, (int) (*it - position), 1.0f);
                    it = pending.erase (it);
                }
                else
                {
                    ++it;
                }
            }
            position += n;
        }

        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

    private:
        const int latency;
        std::int64_t position = 0;
        std::vector<std::int64_t> pending; // só no teste: alocar aqui não importa
    };

    RecordTargets targetsFor (DiskWriter* drum, DiskWriter* chrome, int pluginLatency)
    {
        RecordTargets t;
        t.drum = drum;
        t.chrome = chrome;
        t.pluginLatency = pluginLatency;
        return t;
    }

    MidiEvent noteOn (int note)
    {
        MidiEvent e;
        e.size = 3;
        e.data[0] = 0x99;
        e.data[1] = (std::uint8_t) note;
        e.data[2] = 100;
        e.timeSec = juce::Time::getMillisecondCounterHiRes() * 0.001;
        return e;
    }

    struct Harness
    {
        explicit Harness (int pluginLatency)
        {
            engine.setPlugin (std::make_unique<ImpulsePlugin> (pluginLatency));
            engine.setMidiTiming (AudioEngine::MidiTiming::lowestLatency); // nota no início do bloco seguinte
            engine.audioDeviceAboutToStart (&device);
            out.setSize (2, testBlock);
        }

        ~Harness() { engine.audioDeviceStopped(); }

        // Roda um bloco; devolve as posições (no bloco) onde a saída teve impulso.
        // `input` (opcional): canal de entrada 1 da "Behringer" neste bloco.
        std::vector<int> runBlock (const float* input = nullptr)
        {
            out.clear();
            const float* inputs[] = { input };
            engine.audioDeviceIOCallbackWithContext (input != nullptr ? inputs : nullptr, input != nullptr ? 1 : 0,
                                                     out.getArrayOfWritePointers(), 2, testBlock, {});
            std::vector<int> hits;
            for (int i = 0; i < testBlock; ++i)
                if (out.getSample (0, i) > 0.5f)
                    hits.push_back (i);
            return hits;
        }

        FakeDevice device;
        AudioEngine engine;
        juce::AudioBuffer<float> out;
    };

    std::vector<int> impulsesInWav (const juce::File& file, int& totalSamples)
    {
        std::vector<int> hits;
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (new juce::FileInputStream (file), true));
        if (reader == nullptr)
            return hits;

        totalSamples = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> data ((int) reader->numChannels, totalSamples);
        reader->read (&data, 0, totalSamples, 0, true, true);

        for (int i = 0; i < totalSamples; ++i)
            if (data.getSample (0, i) > 0.5f && data.getSample (1, i) > 0.5f)
                hits.push_back (i);
        return hits;
    }
}

class RecordingSyncTest final : public juce::UnitTest
{
public:
    RecordingSyncTest() : juce::UnitTest (L"Gravação sincronizada", "rec") {}

    void runTest() override
    {
        for (const int latency : { 0, 64 })
        {
            beginTest (L"MIDI x WAV alinhados, latência do plugin " + juce::String (latency));

            Harness h (latency);
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("backbeat-sync-" + juce::String (latency) + ".wav");
            juce::String error;
            auto writer = DiskWriter::createWav (file, testRate, 2, 24, 2.0, error);
            expect (writer != nullptr, error);
            if (writer == nullptr)
                return;

            h.runBlock(); // bloco antes de gravar
            expect (h.engine.requestRecord (targetsFor (writer.get(), nullptr, latency)));

            // Notas chegam antes dos blocos 0, 3, 3 (duas no mesmo bloco) e 10 da gravação.
            const std::vector<int> notesBeforeBlock { 0, 3, 3, 10 };
            for (int block = 0; block < 20; ++block)
            {
                for (const int b : notesBeforeBlock)
                    if (b == block)
                        h.engine.pushMidi (noteOn (38));
                h.runBlock();
            }

            h.engine.requestStop();
            h.runBlock();
            expect (h.engine.takeResourcesReleased(), L"a thread de áudio precisa liberar o writer");
            expect (h.engine.getTransportState() == TransportState::idle);
            writer->finish();

            std::vector<std::uint64_t> midiPositions;
            TimedMidi m;
            while (h.engine.popRecordedMidi (m))
                if ((m.data[0] & 0xF0) == 0x90)
                    midiPositions.push_back (m.samplePos);

            int total = 0;
            const auto audioHits = impulsesInWav (file, total);

            expectEquals ((int) midiPositions.size(), 4);
            expectEquals (total, 20 * testBlock - latency); // 20 blocos, menos o trecho compensado
            expectEquals ((int) midiPositions.front(), 0);
            expectEquals ((int) midiPositions.back(), 10 * testBlock);

            // Duas notas no mesmo bloco caem na mesma amostra → um impulso só no WAV.
            std::vector<int> uniqueMidi;
            for (auto p : midiPositions)
                if (uniqueMidi.empty() || uniqueMidi.back() != (int) p)
                    uniqueMidi.push_back ((int) p);

            expect (audioHits == uniqueMidi, "impulsos do WAV precisam cair nas mesmas amostras do MIDI");
            file.deleteFile();
        }

        beginTest (L"bloco sem plugin vira silêncio (WAV não encurta)");
        {
            Harness h (0);
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("backbeat-silencio.wav");
            juce::String error;
            auto writer = DiskWriter::createWav (file, testRate, 2, 24, 2.0, error);
            expect (h.engine.requestRecord (targetsFor (writer.get(), nullptr, 0)));
            h.runBlock();
            h.engine.setPlugin (nullptr);   // simula troca de plugin no meio da gravação
            h.runBlock();
            h.runBlock();
            h.engine.requestStop();
            h.runBlock();
            writer->finish();
            int total = 0;
            impulsesInWav (file, total);
            expectEquals (total, 3 * testBlock);
            file.deleteFile();
        }

        beginTest (L"chrome.wav: mesmo início e tamanho da bateria, com o áudio do Chrome");
        {
            Harness h (0);
            const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory);
            const auto drumFile = dir.getChildFile ("backbeat-bateria.wav");
            const auto chromeFile = dir.getChildFile ("backbeat-chrome.wav");
            juce::String error;
            auto drum = DiskWriter::createWav (drumFile, testRate, 2, 24, 2.0, error);
            auto chrome = DiskWriter::createWav (chromeFile, testRate, 2, 24, 2.0, error);

            LoopbackStream stream ((int) testRate);
            h.engine.setChromeStream (&stream);

            std::vector<float> tone ((size_t) testBlock * 2, 0.5f); // "música" constante, mesma taxa
            const auto feed = [&] { stream.pushInterleaved (tone.data(), testBlock, 2); };

            for (int i = 0; i < 10; ++i) { feed(); h.runBlock(); }   // Chrome já tocando antes de gravar

            expect (h.engine.requestRecord (targetsFor (drum.get(), chrome.get(), 0)));
            for (int i = 0; i < 40; ++i) { feed(); h.runBlock(); }
            h.engine.requestStop();
            feed();
            h.runBlock();
            drum->finish();
            chrome->finish();
            h.engine.setChromeStream (nullptr);

            int drumTotal = 0, chromeTotal = 0;
            impulsesInWav (drumFile, drumTotal);
            impulsesInWav (chromeFile, chromeTotal);
            expectEquals (chromeTotal, drumTotal, "as duas faixas precisam ter o mesmo tamanho");
            expectEquals (chromeTotal, 40 * testBlock);

            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (new juce::FileInputStream (chromeFile), true));
            juce::AudioBuffer<float> data (2, juce::jmax (1, chromeTotal));
            if (reader != nullptr && chromeTotal > 0)
            {
                reader->read (&data, 0, chromeTotal, 0, true, true);
                expectWithinAbsoluteError (data.getSample (0, 0), 0.5f, 0.01f);   // já começa com som
                expectWithinAbsoluteError (data.getSample (1, chromeTotal - 1), 0.5f, 0.01f);
            }
            reader.reset();
            drumFile.deleteFile();
            chromeFile.deleteFile();
        }

        beginTest (L"parar antes de a gravação começar");
        {
            Harness h (0);
            juce::String error;
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("backbeat-cancelado.wav");
            auto writer = DiskWriter::createWav (file, testRate, 2, 24, 2.0, error);
            expect (h.engine.requestRecord (targetsFor (writer.get(), nullptr, 0)));
            h.engine.requestStop(); // antes de qualquer bloco
            expect (h.engine.takeResourcesReleased(), "pedido cancelado libera os recursos na hora");
            h.runBlock();
            expect (h.engine.getTransportState() == TransportState::idle);
            writer->finish();
            file.deleteFile();
        }

        beginTest (L"reprodução toca cada nota na amostra certa");
        {
            Harness h (0);
            h.runBlock();

            std::vector<TimedMidi> events;
            for (const std::uint64_t pos : { 10ull, 300ull, 1000ull })
            {
                TimedMidi e;
                e.samplePos = pos;
                e.size = 3;
                e.data[0] = 0x99;
                e.data[1] = 38;
                e.data[2] = 90;
                events.push_back (e);
            }

            expect (h.engine.requestPlay (events.data(), (int) events.size()));

            std::vector<int> hits;
            for (int block = 0; block < 8; ++block)
                for (int i : h.runBlock())
                    hits.push_back (block * testBlock + i);

            expect (hits == std::vector<int> { 10, 300, 1000 }, L"posições tocadas diferentes das gravadas");
            expect (h.engine.getTransportState() == TransportState::idle, "fim do take volta para parado");
            expect (h.engine.takeResourcesReleased());
        }
    }
};

class TakeFilesTest final : public juce::UnitTest
{
public:
    TakeFilesTest() : juce::UnitTest ("Arquivos do take", "rec") {}

    static TakeEvent ev (double t, std::uint8_t status, std::uint8_t note, std::uint8_t vel)
    {
        TakeEvent e;
        e.timeSec = t;
        e.size = 3;
        e.data[0] = status;
        e.data[1] = note;
        e.data[2] = vel;
        return e;
    }

    void runTest() override
    {
        beginTest ("note-off que faltam");
        {
            std::vector<TakeEvent> events { ev (0.0, 0x99, 38, 100), ev (0.2, 0x99, 38, 90), ev (0.3, 0x99, 36, 80), ev (0.31, 0x89, 36, 0) };
            takefiles::addMissingNoteOffs (events, 0.05);

            int ons = 0, offs = 0;
            for (const auto& e : events)
            {
                if ((e.data[0] & 0xF0) == 0x90 && e.data[2] > 0) ++ons;
                if ((e.data[0] & 0xF0) == 0x80) ++offs;
            }
            expectEquals (ons, 3);
            expectEquals (offs, 3); // 2 criados para a caixa + o do bumbo que já existia

            bool sorted = true;
            for (size_t i = 1; i < events.size(); ++i)
                sorted = sorted && events[i - 1].timeSec <= events[i].timeSec;
            expect (sorted);
        }

        beginTest (".mid ida e volta");
        {
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("backbeat-take.mid");
            std::vector<TakeEvent> events { ev (0.0, 0x99, 36, 100), ev (0.5, 0x99, 38, 110), ev (1.25, 0xB9, 4, 127), ev (1.5, 0x99, 42, 70) };
            expect (takefiles::writeMidiFile (file, events, 120.0, "Bateria"));

            const auto back = takefiles::readMidiFile (file);
            std::vector<double> onTimes;
            for (const auto& e : back)
                if ((e.data[0] & 0xF0) == 0x90 && e.data[2] > 0)
                    onTimes.push_back (e.timeSec);

            expectEquals ((int) onTimes.size(), 3);
            if (onTimes.size() == 3)
            {
                expectWithinAbsoluteError (onTimes[0], 0.0, 0.001);
                expectWithinAbsoluteError (onTimes[1], 0.5, 0.001);
                expectWithinAbsoluteError (onTimes[2], 1.5, 0.001);
            }

            bool hasCc = false;
            for (const auto& e : back)
                hasCc = hasCc || ((e.data[0] & 0xF0) == 0xB0 && e.data[1] == 4 && e.data[2] == 127);
            expect (hasCc, "CC4 do chimbal precisa ir para o .mid");
            file.deleteFile();
        }

        beginTest ("DiskWriter: escreve tudo e conta perdas");
        {
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("backbeat-disk.wav");
            juce::String error;
            auto w = DiskWriter::createWav (file, testRate, 2, 24, 0.1, error); // FIFO pequeno (~4410 amostras)
            expect (w != nullptr, error);

            std::vector<float> ramp (1000);
            for (size_t i = 0; i < ramp.size(); ++i)
                ramp[i] = (float) i / 2000.0f;
            const float* ch[] = { ramp.data(), ramp.data() };

            for (int i = 0; i < 20; ++i)
            {
                w->write (ch, 2, 1000);
                juce::Thread::sleep (10);
            }
            w->finish();

            int total = 0;
            impulsesInWav (file, total);
            expectEquals ((juce::int64) total + (juce::int64) w->getDroppedSamples(), (juce::int64) 20000);
            expectEquals ((int) w->getDroppedSamples(), 0);
            file.deleteFile();
        }
    }
};

// ---------------------------------------------------------------------------
// F5: mixer e entradas da Behringer
// ---------------------------------------------------------------------------

class MixerTest final : public juce::UnitTest
{
public:
    MixerTest() : juce::UnitTest ("Mixer e entradas", "engine") {}

    void runTest() override
    {
        beginTest ("mudo, solo, master e monitor da entrada");
        {
            Harness h (0);
            LoopbackStream stream ((int) testRate);
            h.engine.setChromeStream (&stream);

            std::vector<float> chromeTone ((size_t) testBlock * 2, 0.5f);
            std::vector<float> guitar ((size_t) testBlock, 0.2f);
            const auto block = [&] (bool withInput)
            {
                stream.pushInterleaved (chromeTone.data(), testBlock, 2);
                h.runBlock (withInput ? guitar.data() : nullptr);
                return h.out.getSample (0, testBlock / 2); // saída esquerda no meio do bloco
            };

            for (int i = 0; i < 12; ++i)
                block (false);                                   // resampler do Chrome enche e começa

            expectWithinAbsoluteError (block (false), 0.5f, 0.01f);       // Chrome sozinho

            h.engine.setTrackMute (trackChrome, true);
            expectWithinAbsoluteError (block (false), 0.0f, 0.001f);      // mudo
            h.engine.setTrackMute (trackChrome, false);

            h.engine.setTrackSolo (trackInput1, true);
            expectWithinAbsoluteError (block (false), 0.0f, 0.001f);      // solo em outra faixa cala o Chrome
            h.engine.setTrackSolo (trackChrome, true);
            expectWithinAbsoluteError (block (false), 0.5f, 0.01f);       // as duas em solo
            h.engine.setTrackSolo (trackInput1, false);
            h.engine.setTrackSolo (trackChrome, false);

            h.engine.setMasterGain (0.5f);
            expectWithinAbsoluteError (block (false), 0.25f, 0.01f);      // master
            h.engine.setMasterGain (1.0f);

            // Entrada sem "Ouvir": não aparece na saída, mas o VU (antes do fader) mede.
            h.engine.takeTrackPeak (trackInput1);
            h.engine.setTrackGain (trackInput1, 0.0f);
            expectWithinAbsoluteError (block (true), 0.5f, 0.01f);
            expectWithinAbsoluteError (h.engine.takeTrackPeak (trackInput1), 0.2f, 0.001f);

            h.engine.setTrackGain (trackInput1, 1.0f);
            h.engine.setInputMonitor (0, true);
            expectWithinAbsoluteError (block (true), 0.7f, 0.01f);        // Chrome + guitarra

            h.engine.setChromeStream (nullptr);
        }

        beginTest (L"entrada gravada adiantada pela latência e com o tamanho da bateria");
        {
            Harness h (0);
            const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory);
            const auto drumFile = dir.getChildFile ("backbeat-f5-bateria.wav");
            const auto inFile = dir.getChildFile ("backbeat-f5-entrada.wav");
            juce::String error;
            auto drum = DiskWriter::createWav (drumFile, testRate, 2, 24, 2.0, error);
            auto input = DiskWriter::createWav (inFile, testRate, 1, 24, 2.0, error);

            constexpr int compensation = 500;     // ex.: 6,8 ms de entrada + 4,5 ms de saída
            constexpr int hitAt = 3000;           // posição (desde o início da gravação) em que a guitarra chega

            RecordTargets t;
            t.drum = drum.get();
            t.inputs[0] = input.get();
            t.inputCompensation = compensation;
            expect (h.engine.requestRecord (t));

            std::vector<float> in ((size_t) testBlock);
            constexpr int blocks = 30;
            for (int b = 0; b < blocks; ++b)
            {
                std::fill (in.begin(), in.end(), 0.0f);
                if (hitAt / testBlock == b)
                    in[(size_t) (hitAt % testBlock)] = 0.9f;
                h.runBlock (in.data());
            }

            h.engine.requestStop();
            h.runBlock (in.data());
            expect (h.engine.takeResourcesReleased());

            input->writeSilence (compensation);   // o Recorder completa o fim assim
            drum->finish();
            input->finish();

            int drumTotal = 0, inTotal = 0;
            impulsesInWav (drumFile, drumTotal);

            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (new juce::FileInputStream (inFile), true));
            expect (reader != nullptr);
            if (reader != nullptr)
            {
                inTotal = (int) reader->lengthInSamples;
                juce::AudioBuffer<float> data (1, inTotal);
                reader->read (&data, 0, inTotal, 0, true, false);
                int found = -1;
                for (int i = 0; i < inTotal; ++i)
                    if (data.getSample (0, i) > 0.5f)
                        found = i;
                expectEquals (found, hitAt - compensation, L"a guitarra precisa cair adiantada pela latência");
                expectEquals ((int) reader->numChannels, 1);
            }
            reader.reset();

            expectEquals (inTotal, drumTotal, "entrada e bateria com o mesmo tamanho");
            expectEquals (drumTotal, blocks * testBlock);
            drumFile.deleteFile();
            inFile.deleteFile();
        }
    }
};

static RecordingSyncTest recordingSyncTest;
static TakeFilesTest takeFilesTest;
static MixerTest mixerTest;

} // namespace bb
