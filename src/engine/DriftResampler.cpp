#include "engine/DriftResampler.h"

#include <cmath>

namespace bb
{

namespace
{
    constexpr double maxCorrection = 0.005;  // ±5000 ppm: bem acima de qualquer cristal real (±100–300 ppm)
    constexpr double smoothingSeconds = 1.0; // média do nível do FIFO (os pacotes WASAPI chegam a cada ~10 ms)
}

void DriftResampler::prepare (double sr, int maxBlockSize, double latencyMs)
{
    sampleRate = sr;
    targetMs = latencyMs;
    targetFrames = sr * latencyMs / 1000.0;

    // PI criticamente amortecido: erro de nível e em quadros, desvio da razão = kp*e + ki*∫e dt.
    // Malha: de/dt = -sr*(kp*e + ki*∫e)  →  constante de tempo ~10 s, sem oscilar.
    const double omega = 0.1;              // 1/s
    kp = omega / sr;
    ki = omega * omega / (4.0 * sr);
    integralLimit = maxCorrection / ki;

    maxBlock = maxBlockSize;
    const size_t capacity = (size_t) std::ceil (maxBlockSize * (1.0 + maxCorrection)) + 64;
    inL.assign (capacity, 0.0f);
    inR.assign (capacity, 0.0f);

    state = false;
    integral = 0.0;
    ratio = 1.0;
    running.store (false);
    resetRequested.store (false);
}

void DriftResampler::startRunning (int available) noexcept
{
    // O integral NÃO é zerado: ele guarda a diferença de relógio entre os dispositivos, que não muda
    // quando o Chrome pausa. Só prepare()/requestReset() zeram.
    interpL.reset();
    interpR.reset();
    smoothFill = available;
    state = true;
    running.store (true);
}

bool DriftResampler::process (LoopbackStream* stream, float* outL, float* outR, int n) noexcept
{
    const auto silence = [&]
    {
        juce::FloatVectorOperations::clear (outL, n);
        juce::FloatVectorOperations::clear (outR, n);
        return false;
    };

    if (resetRequested.exchange (false) || stream == nullptr)
    {
        state = false;
        integral = 0.0;
        ratio = 1.0;
        running.store (false);
        if (stream == nullptr)
            return silence();
    }

    if (n <= 0 || n > maxBlock)
        return silence();

    const int available = stream->getNumReady();

    if (! state)
    {
        // Enche até o alvo antes de começar (ex.: Chrome voltou a tocar depois de um silêncio sem pacotes).
        fillMs.store (available * 1000.0 / sampleRate);
        if (available < (int) targetFrames)
            return silence();
        startRunning (available);
    }

    // Excesso grande (ex.: a thread de áudio ficou parada): descarta e volta ao alvo.
    if (available > targetFrames + sampleRate * 0.25)
    {
        stream->consume (available - (int) targetFrames);
        startRunning ((int) targetFrames);
        resyncs.fetch_add (1, std::memory_order_relaxed);
    }

    const double dt = n / sampleRate;
    const double alpha = 1.0 - std::exp (-dt / smoothingSeconds);
    smoothFill += (stream->getNumReady() - smoothFill) * alpha;

    const double error = smoothFill - targetFrames;
    integral = juce::jlimit (-integralLimit, integralLimit, integral + error * dt);
    ratio = 1.0 + juce::jlimit (-maxCorrection, maxCorrection, kp * error + ki * integral);

    const int needed = (int) std::ceil (n * ratio) + 2;
    if (stream->getNumReady() < needed || ! stream->peek (inL.data(), inR.data(), needed))
    {
        // Faltou áudio (o Chrome parou de mandar pacotes): silêncio e volta a encher.
        underruns.fetch_add (1, std::memory_order_relaxed);
        state = false;
        running.store (false);
        return silence();
    }

    const int used = interpL.process (ratio, inL.data(), outL, n);
    interpR.process (ratio, inR.data(), outR, n);
    stream->consume (used);

    ratioPpm.store ((ratio - 1.0) * 1.0e6, std::memory_order_relaxed);
    fillMs.store (smoothFill * 1000.0 / sampleRate, std::memory_order_relaxed);
    return true;
}

} // namespace bb
