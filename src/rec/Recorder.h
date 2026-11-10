#pragma once

#include <juce_core/juce_core.h>

#include "engine/AudioEngine.h"
#include "rec/DiskWriter.h"
#include "rec/TakeFiles.h"

#include <functional>
#include <memory>
#include <vector>

namespace bb
{

// Orquestra gravação e reprodução na thread de mensagens (o AudioEngine faz a parte de tempo real).
//
// Cada gravação vira uma pasta <pasta de gravações>/AAAA-MM-DD HH-MM-SS/ com:
//   bateria.mid   notas que o SSD5 recebeu (depois do mapeamento), canal original
//   bateria.wav   saída do SSD5, 24 bits estéreo, alinhada amostra a amostra com o .mid
//   sessao.json   metadados (taxa, duração, perfil, kit, perdas)
class Recorder
{
public:
    struct Metadata
    {
        juce::String profile, kit, plugin, midiTiming;
        double bpm = 120.0;
        int pluginLatencySamples = 0;
        bool recordChrome = false;     // gravar também o chrome.wav (captura ligada)
        juce::String chromeProcess;

        // Entradas da Behringer (F5): armadas para gravar, nome e compensação de latência.
        bool recordInput[numInputTracks] {};
        juce::String inputName[numInputTracks];
        int inputCompensationSamples = 0;   // entrada + saída (+ ajuste fino), em amostras
    };

    struct Take
    {
        juce::File folder;
        std::vector<TakeEvent> events; // em segundos
        double durationSec = 0.0;
        int noteCount = 0;
    };

    enum class State { idle, recording, playing, finishing };

    explicit Recorder (AudioEngine&);
    ~Recorder();

    bool startRecording (const juce::File& recordingsDir, const Metadata&, juce::String& error);
    bool startPlayback (juce::String& error); // toca o último take pelo plugin (com o kit atual)
    void stop();

    // Chamar ~30x/s: recolhe o MIDI gravado e finaliza quando a thread de áudio libera os recursos.
    void poll();

    State getState() const noexcept { return state; }
    double getPositionSeconds() const;
    int getRecordingNoteCount() const noexcept { return liveNoteCount; }
    float getDiskFillLevel() const noexcept { return drumWriter != nullptr ? drumWriter->getFillLevel() : 0.0f; }

    const Take* getLastTake() const noexcept { return lastTake.get(); }
    bool loadLatestTake (const juce::File& recordingsDir); // ao abrir o app

    std::function<void (const juce::String& message, bool isError)> onMessage;
    std::function<void()> onStateChanged;

    static juce::File defaultRecordingsDirectory();

private:
    void drainRecordedMidi();
    void finishRecording();
    void setState (State s);
    void message (const juce::String& text, bool isError = false) { if (onMessage) onMessage (text, isError); }

    AudioEngine& engine;
    State state = State::idle;
    State finishingFrom = State::idle;

    // Gravação em andamento
    std::unique_ptr<DiskWriter> drumWriter, chromeWriter;
    std::unique_ptr<DiskWriter> inputWriters[numInputTracks];
    void finishAllWriters();
    std::uint32_t chromeUnderrunsAtStart = 0;
    juce::File currentFolder;
    Metadata currentMeta;
    juce::Time startedAt;
    double recordSampleRate = 44100.0;
    std::vector<TimedMidi> recorded;
    int liveNoteCount = 0;
    std::uint32_t midiDroppedAtStart = 0;

    // Reprodução
    std::vector<TimedMidi> playback;

    std::unique_ptr<Take> lastTake;
};

} // namespace bb
