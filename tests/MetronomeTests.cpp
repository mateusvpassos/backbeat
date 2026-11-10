// Testes da F6: metrônomo, contagem antes de gravar e exportação da mixagem.

#include <juce_audio_formats/juce_audio_formats.h>

#include "engine/AudioEngine.h"
#include "rec/DiskWriter.h"
#include "rec/MixExporter.h"

#include <cmath>
#include <vector>

namespace bb
{

namespace
{
    constexpr double rate = 44100.0;
    constexpr int block = 256;

    class SilentDevice final : public juce::AudioIODevice
    {
    public:
        SilentDevice() : juce::AudioIODevice ("Falso", "Teste") {}
        juce::StringArray getOutputChannelNames() override { return { "1", "2" }; }
        juce::StringArray getInputChannelNames() override { return {}; }
        juce::Array<double> getAvailableSampleRates() override { return { rate }; }
        juce::Array<int> getAvailableBufferSizes() override { return { block }; }
        int getDefaultBufferSize() override { return block; }
        juce::String open (const juce::BigInteger&, const juce::BigInteger&, double, int) override { return {}; }
        void close() override {}
        bool isOpen() override { return true; }
        void start (juce::AudioIODeviceCallback*) override {}
        void stop() override {}
        bool isPlaying() override { return true; }
        juce::String getLastError() override { return {}; }
        int getCurrentBufferSizeSamples() override { return block; }
        double getCurrentSampleRate() override { return rate; }
        int getCurrentBitDepth() override { return 32; }
        juce::BigInteger getActiveOutputChannels() const override { return 3; }
        juce::BigInteger getActiveInputChannels() const override { return 0; }
        int getOutputLatencyInSamples() override { return 0; }
        int getInputLatencyInSamples() override { return 0; }
    };

    // Motor sem plugin (a bateria grava silêncio); guarda toda a saída do canal esquerdo.
    struct Rig
    {
        Rig()  { engine.audioDeviceAboutToStart (&device); out.setSize (2, block); }
        ~Rig() { engine.audioDeviceStopped(); }

        void run (int blocks)
        {
            for (int b = 0; b < blocks; ++b)
            {
                out.clear();
                engine.audioDeviceIOCallbackWithContext (nullptr, 0, out.getArrayOfWritePointers(), 2, block, {});
                output.insert (output.end(), out.getReadPointer (0), out.getReadPointer (0) + block);
            }
        }

        // Começo de cada clique: primeira amostra audível depois de um trecho de silêncio.
        std::vector<int> onsets() const
        {
            std::vector<int> result;
            int quiet = 1 << 20;
            for (int i = 0; i < (int) output.size(); ++i)
            {
                if (std::abs (output[(size_t) i]) > 1.0e-5f)
                {
                    if (quiet > 500)
                        result.push_back (i - 1); // o clique começa em zero (seno): a 1ª amostra é 0
                    quiet = 0;
                }
                else
                {
                    ++quiet;
                }
            }
            return result;
        }

        float peakAfter (int start, int len) const
        {
            float p = 0.0f;
            for (int i = start; i < start + len && i < (int) output.size(); ++i)
                p = juce::jmax (p, std::abs (output[(size_t) i]));
            return p;
        }

        SilentDevice device;
        AudioEngine engine;
        juce::AudioBuffer<float> out;
        std::vector<float> output;
    };

    juce::File tempFolder (const juce::String& name)
    {
        auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name);
        f.deleteRecursively();
        f.createDirectory();
        return f;
    }

    void writeConstantWav (const juce::File& file, int channels, int length, float left, float right)
    {
        juce::AudioBuffer<float> data (channels, length);
        for (int i = 0; i < length; ++i)
        {
            data.setSample (0, i, left);
            if (channels > 1)
                data.setSample (1, i, right);
        }
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
        auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions()
                                                  .withSampleRate (rate)
                                                  .withNumChannels (channels)
                                                  .withBitsPerSample (24));
        w->writeFromAudioSampleBuffer (data, 0, length);
    }

    juce::AudioBuffer<float> readWav (const juce::File& file)
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> r (wav.createReaderFor (new juce::FileInputStream (file), true));
        juce::AudioBuffer<float> data;
        if (r == nullptr)
            return data;
        data.setSize ((int) r->numChannels, (int) r->lengthInSamples);
        r->read (&data, 0, (int) r->lengthInSamples, 0, true, true);
        return data;
    }
}

class MetronomeTest final : public juce::UnitTest
{
public:
    MetronomeTest() : juce::UnitTest (L"Metrônomo e contagem", "rec") {}

    void runTest() override
    {
        constexpr double spb = 60.0 * rate / 120.0; // 22050 amostras por tempo

        beginTest (L"desligado não faz som");
        {
            Rig rig;
            rig.run (400);
            expect (rig.onsets().empty());
        }

        beginTest (L"120 BPM: um clique a cada 22050 amostras, tempo 1 mais forte");
        {
            Rig rig;
            rig.engine.setTempo (120.0);
            rig.engine.setBeatsPerBar (4);
            rig.engine.setMetronomeOn (true);
            rig.run ((int) (spb * 6 / block));

            const auto on = rig.onsets();
            expectEquals ((int) on.size(), 6);
            for (int k = 0; k < (int) on.size(); ++k)
                expect (std::abs (on[(size_t) k] - std::llround (k * spb)) <= 1,
                        "clique " + juce::String (k) + " em " + juce::String (on[(size_t) k]));

            if (on.size() >= 5)
            {
                const float accent = rig.peakAfter (on[0], 1000);
                expect (accent > rig.peakAfter (on[1], 1000) + 0.05f, L"tempo 1 deveria ser mais forte");
                expectWithinAbsoluteError (rig.peakAfter (on[4], 1000), accent, 0.01f);
            }
        }

        beginTest (L"mudar o andamento não solta clique a mais");
        {
            Rig rig;
            rig.engine.setTempo (120.0);
            rig.engine.setMetronomeOn (true);
            rig.run ((int) (spb * 1.5 / block));
            rig.engine.setTempo (180.0);
            rig.run ((int) (spb * 4 / block));

            const auto on = rig.onsets();
            const double fastSpb = 60.0 * rate / 180.0;
            for (size_t k = 1; k < on.size(); ++k)
                expect (on[k] - on[k - 1] >= (int) fastSpb - 2, "cliques perto demais: " + juce::String (on[k] - on[k - 1]));
            expect (on.size() >= 6);
        }

        beginTest (L"contagem de 1 compasso: 4 cliques e a gravação começa no tempo 1");
        {
            Rig rig;
            rig.engine.setTempo (120.0);
            rig.engine.setBeatsPerBar (4);
            rig.engine.setMetronomeOn (true); // ligado: o tempo 1 do take também clica

            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("backbeat-countin.wav");
            juce::String error;
            auto writer = DiskWriter::createWav (file, rate, 2, 24, 4.0, error);
            expect (writer != nullptr, error);
            if (writer == nullptr)
                return;

            RecordTargets t;
            t.drum = writer.get();
            t.countInBeats = 4;
            expect (rig.engine.requestRecord (t));

            const int countInBlocks = (int) std::ceil (std::ceil (4 * spb) / block); // 345
            const int countInSamples = countInBlocks * block;                     // 88320

            rig.run (1);
            expect (rig.engine.getTransportState() == TransportState::countIn);
            rig.run (countInBlocks - 1);
            expect (rig.engine.getTransportState() == TransportState::countIn);
            expectEquals ((int) rig.engine.getTransportPosition(), 0);
            expectEquals ((int) writer->getWrittenSamples(), 0);
            expect (rig.engine.getMetronomeBeat() == -1);

            rig.run (100);
            expect (rig.engine.getTransportState() == TransportState::recording);
            expectEquals ((int) rig.engine.getTransportPosition(), 100 * block);

            rig.engine.requestStop();
            rig.run (1);
            expect (rig.engine.takeResourcesReleased());
            writer->finish();
            expectEquals ((int) writer->getWrittenSamples(), 100 * block);

            const auto on = rig.onsets();
            expectEquals ((int) on.size(), 6); // 4 da contagem + tempos 0 e 1 do take (100 blocos = 25600)
            const int first = countInSamples - (int) std::llround (4 * spb);
            for (int k = 0; k < 4 && k < (int) on.size(); ++k)
                expect (std::abs (on[(size_t) k] - (first + (int) std::llround (k * spb))) <= 1,
                        "contagem " + juce::String (k) + " em " + juce::String (on[(size_t) k]));
            if (on.size() >= 5)
                expectEquals (on[4], countInSamples); // tempo 1 do take = 1ª amostra gravada
            if (on.size() >= 1)
                expect (rig.peakAfter (on[0], 1000) > rig.peakAfter (on[1], 1000) + 0.05f, L"contagem começa no tempo 1");
            file.deleteFile();
        }

        beginTest (L"a contagem clica mesmo com o metrônomo desligado; parar nela não grava nada");
        {
            Rig rig;
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("backbeat-countin2.wav");
            juce::String error;
            auto writer = DiskWriter::createWav (file, rate, 2, 24, 4.0, error);
            if (writer == nullptr)
                return;

            RecordTargets t;
            t.drum = writer.get();
            t.countInBeats = 8;
            expect (rig.engine.requestRecord (t));
            rig.run ((int) (spb * 2.5 / block));
            expectEquals ((int) rig.onsets().size(), 3);

            rig.engine.requestStop();
            rig.run (1);
            expect (rig.engine.takeResourcesReleased());
            expect (rig.engine.getTransportState() == TransportState::idle);
            writer->finish();
            expectEquals ((int) writer->getWrittenSamples(), 0);

            const auto before = rig.onsets().size();
            rig.run ((int) (spb * 2 / block));
            expectEquals (rig.onsets().size(), before); // desligado de novo depois de parar
            file.deleteFile();
        }
    }
};

class MixExportTest final : public juce::UnitTest
{
public:
    MixExportTest() : juce::UnitTest (L"Exportar mixagem", "rec") {}

    void runTest() override
    {
        beginTest (L"soma com os volumes; entrada mono nos dois lados; tamanho da faixa mais longa");
        {
            const auto folder = tempFolder ("backbeat-mix-1");
            writeConstantWav (folder.getChildFile ("bateria.wav"), 2, 10000, 0.25f, 0.5f);
            writeConstantWav (folder.getChildFile ("entrada-1.wav"), 1, 6000, 0.1f, 0.0f);
            writeConstantWav (folder.getChildFile ("chrome.wav"), 2, 10000, 0.3f, 0.3f);

            mixexport::Gains g;
            g.track[trackDrums] = 1.0f;
            g.track[trackInput1] = 2.0f;
            g.track[trackChrome] = 0.0f; // muda: fica de fora
            g.master = 1.0f;

            const auto r = mixexport::exportTake (folder, g);
            expect (r.ok, r.error);
            expectEquals (r.tracksUsed, 2);
            expectEquals (r.appliedGain, 1.0f);

            const auto mix = readWav (folder.getChildFile ("mixagem.wav"));
            expectEquals (mix.getNumChannels(), 2);
            expectEquals (mix.getNumSamples(), 10000);
            expectWithinAbsoluteError (mix.getSample (0, 100), 0.45f, 1.0e-4f);
            expectWithinAbsoluteError (mix.getSample (1, 100), 0.70f, 1.0e-4f);
            expectWithinAbsoluteError (mix.getSample (0, 8000), 0.25f, 1.0e-4f); // a entrada acabou
            expectWithinAbsoluteError (mix.getSample (1, 8000), 0.50f, 1.0e-4f);
            expect (! folder.getChildFile ("mixagem.tmp.wav").exists());
            folder.deleteRecursively();
        }

        beginTest (L"se passar de 0 dBFS, abaixa tudo para o pico ficar em -1 dBFS");
        {
            const auto folder = tempFolder ("backbeat-mix-2");
            writeConstantWav (folder.getChildFile ("bateria.wav"), 2, 4000, 0.5f, 0.25f);

            mixexport::Gains g;
            g.track[trackDrums] = 1.0f;
            g.master = 4.0f; // pico 2.0

            const auto r = mixexport::exportTake (folder, g);
            expect (r.ok, r.error);
            expectWithinAbsoluteError (r.peak, 2.0f, 1.0e-3f);

            const auto mix = readWav (folder.getChildFile ("mixagem.wav"));
            expectWithinAbsoluteError (mix.getMagnitude (0, mix.getNumSamples()), 0.8913f, 1.0e-3f);
            expectWithinAbsoluteError (mix.getSample (1, 10) / mix.getSample (0, 10), 0.5f, 1.0e-3f); // proporção mantida
            folder.deleteRecursively();
        }

        beginTest (L"take sem áudio ou tudo mudo: erro claro, nenhum arquivo");
        {
            const auto folder = tempFolder ("backbeat-mix-3");
            mixexport::Gains g;
            auto r = mixexport::exportTake (folder, g);
            expect (! r.ok && r.error.isNotEmpty());

            writeConstantWav (folder.getChildFile ("bateria.wav"), 2, 100, 0.1f, 0.1f);
            g.track[trackDrums] = 0.0f;
            r = mixexport::exportTake (folder, g);
            expect (! r.ok && r.error.isNotEmpty());
            expect (! folder.getChildFile ("mixagem.wav").exists());

            g.track[trackDrums] = 1.0f;
            r = mixexport::exportTake (folder, g, [] (float) { return false; }); // cancelado
            expect (r.cancelled && ! r.ok);
            expect (! folder.getChildFile ("mixagem.wav").exists());
            expect (! folder.getChildFile ("mixagem.tmp.wav").exists());
            folder.deleteRecursively();
        }
    }
};

static MetronomeTest metronomeTest;
static MixExportTest mixExportTest;

} // namespace bb
