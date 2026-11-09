#pragma once

#include "capture/ProcessLoopbackCapture.h"
#include "engine/AudioEngine.h"

namespace bb
{

// Liga/desliga a captura do Chrome e a mantém viva (thread de mensagens):
//   - acha o processo principal (chrome.exe) e captura a árvore dele;
//   - se o Chrome fechar/reabrir ou a captura cair, tenta de novo a cada 2 s;
//   - se a taxa do ASIO mudar, reinicia na taxa nova.
class ChromeCapture
{
public:
    explicit ChromeCapture (AudioEngine&);
    ~ChromeCapture();

    void setEnabled (bool shouldCapture, const juce::String& exeName);
    bool isEnabled() const noexcept { return enabled; }
    bool isCapturing() const noexcept { return capture != nullptr && capture->isRunning(); }
    juce::String getExeName() const { return exeName; }

    void poll();                 // ~2x por segundo
    void restart();              // ex.: taxa do ASIO mudou

    juce::String getStatusText() const { return status; }
    bool hasProblem() const noexcept   { return problem; }

private:
    void tryStart();
    void stopCapture();

    AudioEngine& engine;
    std::unique_ptr<ProcessLoopbackCapture> capture;
    bool enabled = false;
    juce::String exeName { "chrome.exe" };
    juce::String status;
    bool problem = false;
    double captureRate = 0.0;
    double nextRetryMs = 0.0;
};

} // namespace bb
