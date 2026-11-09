// F4: o resampler precisa manter o Chrome (relógio WASAPI) travado no relógio do ASIO em gravações longas.
// Simula 30 minutos de um produtor com relógio diferente, entregando pacotes de 10 ms com atraso irregular.

#include <juce_audio_basics/juce_audio_basics.h>

#include "engine/DriftResampler.h"

#include <cmath>
#include <deque>
#include <vector>

namespace bb
{

class DriftResamplerTest final : public juce::UnitTest
{
public:
    DriftResamplerTest() : juce::UnitTest (L"Deriva de relógio (Chrome x ASIO)", "engine") {}

    struct Result
    {
        double minFillMs = 1e9, maxFillMs = 0.0, finalPpm = 0.0, maxWobblePpm = 0.0;
        std::uint32_t underrunsAfterWarmup = 0, resyncs = 0;
        double producedSec = 0.0, consumedSec = 0.0;
    };

    static Result simulate (double driftPpm, double minutes, juce::Random& random, bool silentGaps = false)
    {
        constexpr double sr = 44100.0;
        constexpr int block = 256;
        constexpr int packet = 441;          // 10 ms no relógio do produtor

        LoopbackStream stream ((int) (sr * 2));
        DriftResampler resampler;
        resampler.prepare (sr, block, 40.0);

        std::vector<float> sine ((size_t) packet * 2);
        for (int i = 0; i < packet; ++i)
            sine[(size_t) i * 2] = sine[(size_t) i * 2 + 1] = (float) std::sin (i * 0.0625);

        std::vector<float> outL (block), outR (block);
        std::deque<int> held;                // pacotes "presos" no Windows por alguns blocos (jitter)
        double producerFrames = 0.0;
        const double producerPerBlock = block * (1.0 + driftPpm * 1e-6);
        const auto totalBlocks = (std::int64_t) (minutes * 60.0 * sr / block);
        const auto warmupBlocks = (std::int64_t) (120.0 * sr / block); // 2 min para convergir

        Result r;
        double ppmSum = 0.0;
        std::int64_t ppmCount = 0;
        std::uint64_t produced = 0;
        std::uint32_t underrunsAtWarmup = 0;

        for (std::int64_t b = 0; b < totalBlocks; ++b)
        {
            // Pausa do Chrome (sem pacotes) por 3 s no minuto 10, se pedido.
            const bool gap = silentGaps && b > (std::int64_t) (600 * sr / block) && b < (std::int64_t) (603 * sr / block);

            producerFrames += producerPerBlock;
            while (producerFrames >= packet)
            {
                producerFrames -= packet;
                if (! gap)
                    held.push_back (random.nextInt (3)); // entregue daqui a 0–2 blocos
            }

            for (auto& h : held)
                --h;
            while (! held.empty() && held.front() < 0)
            {
                held.pop_front();
                stream.pushInterleaved (sine.data(), packet, 2);
                produced += packet;
            }

            resampler.process (&stream, outL.data(), outR.data(), block);

            if (b == warmupBlocks)
                underrunsAtWarmup = resampler.getUnderruns();

            if (b > warmupBlocks && resampler.isRunning())
            {
                r.minFillMs = juce::jmin (r.minFillMs, resampler.getFillMs());
                r.maxFillMs = juce::jmax (r.maxFillMs, resampler.getFillMs());

                // Média da correção nos últimos 5 minutos + quanto ela desvia da deriva real (oscilação).
                if (b > totalBlocks - (std::int64_t) (300.0 * sr / block))
                {
                    ppmSum += resampler.getRatioPpm();
                    ++ppmCount;
                }
                r.maxWobblePpm = juce::jmax (r.maxWobblePpm, std::abs (resampler.getRatioPpm() - driftPpm));
            }
        }

        r.finalPpm = ppmCount > 0 ? ppmSum / ppmCount : 0.0;
        r.underrunsAfterWarmup = resampler.getUnderruns() - underrunsAtWarmup;
        r.resyncs = resampler.getResyncs();
        r.producedSec = produced / sr;
        r.consumedSec = totalBlocks * block / sr;
        return r;
    }

    void runTest() override
    {
        juce::Random random (1234);

        for (const double ppm : { 300.0, -250.0, 40.0 })
        {
            beginTest (L"30 min com relógio " + juce::String (ppm, 0) + " ppm");
            const auto r = simulate (ppm, 30.0, random);

            std::printf ("       nivel %.1f-%.1f ms, correcao media %.1f ppm, oscilacao max %.0f ppm\n",
                         r.minFillMs, r.maxFillMs, r.finalPpm, r.maxWobblePpm);
            // 600 ppm ≈ 1 cent de afinação: inaudível (o ouvido percebe a partir de ~5 cents).
            expect (r.maxWobblePpm < 600.0, L"a correção não pode oscilar a ponto de mexer na afinação");

            expect (r.maxFillMs > 0.0 && r.minFillMs < 1.0e8, "o resampler precisa ter rodado de verdade");
            expectEquals ((int) r.underrunsAfterWarmup, 0, L"faltou áudio depois de convergir");
            expectEquals ((int) r.resyncs, 0, L"não pode precisar descartar áudio");
            expect (r.minFillMs > 25.0 && r.maxFillMs < 55.0, L"nível do FIFO precisa ficar perto dos 40 ms");
            expectWithinAbsoluteError (r.finalPpm, ppm, 25.0);
        }

        beginTest ("pausa do Chrome sem pacotes e volta");
        {
            const auto r = simulate (150.0, 15.0, random, true);
            expect (r.maxFillMs > 0.0 && r.minFillMs < 1.0e8, "o resampler precisa ter rodado de verdade");
            expect (r.underrunsAfterWarmup <= 2, L"a pausa pode causar uma falta, não várias");
            expect (r.minFillMs > 20.0 && r.maxFillMs < 60.0);
            expectWithinAbsoluteError (r.finalPpm, 150.0, 25.0);
        }
    }
};

static DriftResamplerTest driftResamplerTest;

} // namespace bb
