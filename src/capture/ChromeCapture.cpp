#include "capture/ChromeCapture.h"

namespace bb
{

ChromeCapture::ChromeCapture (AudioEngine& e) : engine (e) {}

ChromeCapture::~ChromeCapture()
{
    stopCapture();
}

void ChromeCapture::setEnabled (bool shouldCapture, const juce::String& exe)
{
    exeName = exe.trim().isEmpty() ? juce::String ("chrome.exe") : exe.trim();
    enabled = shouldCapture;
    nextRetryMs = 0.0;

    if (enabled)
        tryStart();
    else
    {
        stopCapture();
        status = "Captura desligada";
        problem = false;
    }
}

void ChromeCapture::stopCapture()
{
    if (capture == nullptr)
        return;

    engine.setChromeStream (nullptr); // depois disso a thread de áudio não toca mais no stream
    capture.reset();
    juce::Logger::writeToLog ("captura do Chrome parada");
}

void ChromeCapture::restart()
{
    if (! enabled)
        return;
    stopCapture();
    nextRetryMs = 0.0;
    tryStart();
}

void ChromeCapture::tryStart()
{
    stopCapture();

    const double sr = engine.getSampleRate();
    if (sr <= 0.0)
    {
        status = L"Esperando o dispositivo de áudio";
        problem = true;
        return;
    }

    const auto pid = ProcessLoopbackCapture::findMainProcess (exeName);
    if (pid == 0)
    {
        status = exeName + L" não está aberto (tentando de novo)";
        problem = true;
        nextRetryMs = juce::Time::getMillisecondCounterHiRes() + 2000.0;
        return;
    }

    auto result = ProcessLoopbackCapture::start (pid, sr);
    if (result.capture == nullptr)
    {
        status = L"Não deu para capturar " + exeName + ": " + result.error;
        problem = true;
        nextRetryMs = juce::Time::getMillisecondCounterHiRes() + 2000.0;
        juce::Logger::writeToLog ("ERRO captura do Chrome: " + result.error);
        return;
    }

    capture = std::move (result.capture);
    captureRate = sr;
    engine.setChromeStream (&capture->getStream());
    problem = false;
    status = "Capturando " + exeName + " (PID " + juce::String (pid) + ")";
    juce::Logger::writeToLog ("captura do Chrome: " + exeName + " PID " + juce::String (pid) + " a "
                              + juce::String ((int) sr) + " Hz");
}

void ChromeCapture::poll()
{
    if (! enabled)
        return;

    const double now = juce::Time::getMillisecondCounterHiRes();

    if (capture != nullptr)
    {
        const bool rateChanged = engine.getSampleRate() > 0.0 && engine.getSampleRate() != captureRate;
        const bool dead = ! capture->isRunning() || ! ProcessLoopbackCapture::isProcessAlive (capture->getProcessId());

        if (rateChanged)
        {
            restart();
            return;
        }

        if (dead)
        {
            const auto why = capture->getFailure();
            stopCapture();
            status = why.isNotEmpty() ? L"A captura caiu: " + why : exeName + juce::String (L" foi fechado (esperando reabrir)");
            problem = true;
            nextRetryMs = now + 2000.0;
            return;
        }

        // Status vivo: tocando ou em silêncio, diferença de relógio e atraso do buffer.
        const auto& rs = engine.getChromeResampler();
        juce::String text = "Capturando " + exeName + " (PID " + juce::String (capture->getProcessId()) + ")";
        if (capture->getSecondsSinceLastSound() > 1.5)
            text << juce::String (L" · sem som agora");
        else if (rs.isRunning())
            text << juce::String (L" · relógio ") << (rs.getRatioPpm() >= 0 ? "+" : "")
                 << juce::String (juce::roundToInt (rs.getRatioPpm())) << L" ppm · buffer "
                 << juce::String (juce::roundToInt (rs.getFillMs())) << " ms";
        if (rs.getUnderruns() > 0)
            text << juce::String (L" · faltas ") << (int) rs.getUnderruns();
        status = text;
        return;
    }

    if (now >= nextRetryMs)
        tryStart();
}

} // namespace bb
