#pragma once

#include <juce_core/juce_core.h>

#include "capture/LoopbackStream.h"

#include <atomic>
#include <memory>

namespace bb
{

// Captura só o áudio de um processo (e dos filhos dele) pelo loopback por processo do WASAPI
// (Windows 10 2004+: ActivateAudioInterfaceAsync + AUDIOCLIENT_PROCESS_LOOPBACK_PARAMS), sem cabo virtual.
//
// O formato é pedido na MESMA taxa do ASIO; o Windows converte (AUTOCONVERTPCM), então só sobra a
// pequena diferença de relógio entre os dois dispositivos, tratada pelo DriftResampler.
// Tudo de COM/WASAPI roda numa thread própria; o áudio sai pelo LoopbackStream (lock-free).
class ProcessLoopbackCapture final : private juce::Thread
{
public:
    struct StartResult
    {
        std::unique_ptr<ProcessLoopbackCapture> capture;
        juce::String error;
    };

    // Thread de mensagens. Bloqueia até a captura começar ou falhar (no máximo ~3 s).
    static StartResult start (std::uint32_t processId, double sampleRate);

    ~ProcessLoopbackCapture() override;

    LoopbackStream& getStream() noexcept { return stream; }
    std::uint32_t getProcessId() const noexcept { return processId; }
    bool isRunning() const noexcept { return running.load() && ! failed.load(); }
    juce::String getFailure() const;
    std::uint64_t getDiscontinuities() const noexcept { return discontinuities.load(); }
    double getSecondsSinceLastSound() const noexcept;

    // Processos
    static std::uint32_t findMainProcess (const juce::String& exeName); // chrome.exe "pai" (não filho de outro chrome.exe)
    static bool isProcessAlive (std::uint32_t processId);

private:
    ProcessLoopbackCapture (std::uint32_t processId, double sampleRate);
    void run() override;
    juce::String setUpAndCapture();   // devolve erro de configuração (vazio se rodou até o fim)

    const std::uint32_t processId;
    const double sampleRate;
    LoopbackStream stream;

    juce::WaitableEvent setupDone;
    juce::String setupError;
    bool setupSignalled = false; // só a thread de captura mexe
    std::atomic<bool> running { false }, failed { false };
    std::atomic<std::uint64_t> discontinuities { 0 };
    std::atomic<double> lastSoundMs { 0.0 };
    void* stopEvent = nullptr; // HANDLE

    mutable juce::CriticalSection failureLock; // só thread de captura x thread de mensagens
    juce::String failure;
};

} // namespace bb
