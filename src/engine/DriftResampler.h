#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "capture/LoopbackStream.h"

#include <atomic>
#include <vector>

namespace bb
{

// Mantém um fluxo de outro relógio (WASAPI) travado no relógio do ASIO.
//
// A taxa nominal já é a mesma (o Windows converte), mas os cristais dos dois dispositivos diferem em
// dezenas a centenas de ppm: sem correção, o FIFO enche ou esvazia aos poucos (100 ppm = 0,36 s por hora).
// Aqui um controle PI olha o nível médio do FIFO e ajusta levemente a razão de um interpolador Lagrange
// para manter o nível no alvo (padrão 40 ms). Resultado: atraso constante, sem estalos, por horas.
//
// process() roda na thread de áudio: sem alocação, sem lock (buffers alocados em prepare()).
class DriftResampler
{
public:
    void prepare (double sampleRate, int maxBlockSize, double targetLatencyMs = 40.0);
    void requestReset() noexcept { resetRequested.store (true); } // qualquer thread

    // Produz exatamente n amostras em outL/outR. Sem dados (ou stream nulo) → silêncio.
    // Devolve true se as amostras vieram do stream.
    bool process (LoopbackStream* stream, float* outL, float* outR, int n) noexcept;

    // Estatísticas (qualquer thread)
    double getRatioPpm() const noexcept   { return ratioPpm.load(); }
    double getFillMs() const noexcept     { return fillMs.load(); }
    double getTargetMs() const noexcept   { return targetMs; }
    std::uint32_t getUnderruns() const noexcept { return underruns.load(); }
    std::uint32_t getResyncs() const noexcept   { return resyncs.load(); }
    bool isRunning() const noexcept       { return running.load(); }

private:
    void startRunning (int available) noexcept;

    double sampleRate = 44100.0;
    int maxBlock = 0;
    double targetMs = 40.0;
    double targetFrames = 1764.0;
    double kp = 0.0, ki = 0.0, integralLimit = 0.0;

    juce::LagrangeInterpolator interpL, interpR;
    std::vector<float> inL, inR;

    bool state = false;           // true = rodando (só a thread de áudio)
    double smoothFill = 0.0, integral = 0.0, ratio = 1.0;

    std::atomic<bool> resetRequested { false }, running { false };
    std::atomic<double> ratioPpm { 0.0 }, fillMs { 0.0 };
    std::atomic<std::uint32_t> underruns { 0 }, resyncs { 0 };
};

} // namespace bb
