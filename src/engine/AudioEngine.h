#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "capture/LoopbackStream.h"
#include "engine/DriftResampler.h"
#include "engine/MidiEvent.h"
#include "util/SpscQueue.h"

#include <atomic>
#include <memory>

namespace bb
{

class DiskWriter;

enum class TransportState
{
    idle = 0,
    recording,
    playing,
    countIn     // contagem antes de gravar (metrônomo tocando, nada sendo gravado ainda)
};

// Faixas do mixer (F5). Tudo sai pelo mesmo par de saídas da Behringer.
enum TrackId
{
    trackDrums = 0,
    trackChrome,
    trackInput1,
    trackInput2,
    numTracks
};
constexpr int numInputTracks = 2;

// O que gravar e onde (writers podem ser nulos). Pertencem a quem pediu até takeResourcesReleased().
struct RecordTargets
{
    DiskWriter* drum = nullptr;
    DiskWriter* chrome = nullptr;
    DiskWriter* inputs[numInputTracks] {};
    int pluginLatency = 0;      // descarta o começo da bateria/Chrome (latência do plugin)
    int inputCompensation = 0;  // adianta as entradas: latência de entrada + saída (+ ajuste fino)
    int countInBeats = 0;       // tempos de contagem antes de começar (0 = grava na hora)
};

// Callback ASIO: recebe o MIDI da fila lock-free, roda o plugin de bateria e manda pra saída.
//
// Regras da thread de áudio (audioDeviceIOCallbackWithContext):
//   - sem alocação, sem lock bloqueante, sem I/O;
//   - o plugin é protegido por um SpinLock usado só com tryLock: se a thread de mensagens
//     estiver trocando o plugin, aquele bloco sai em silêncio em vez de esperar.
class AudioEngine final : public juce::AudioIODeviceCallback
{
public:
    enum class MidiTiming
    {
        lowestLatency = 0, // nota entra no começo do próximo bloco (oscila até 1 buffer)
        steady = 1         // nota entra no ponto exato, com +1 buffer fixo de atraso
    };

    AudioEngine();
    ~AudioEngine() override;

    // ---- thread de mensagens ----
    // Prepara o plugin (se o dispositivo estiver aberto) e troca de forma segura.
    // Devolve o plugin anterior para ser destruído fora da thread de áudio.
    std::unique_ptr<juce::AudioPluginInstance> setPlugin (std::unique_ptr<juce::AudioPluginInstance> newPlugin);
    juce::AudioPluginInstance* getPlugin() const noexcept { return plugin.get(); }

    void setMidiTiming (MidiTiming t) noexcept   { midiTiming.store ((int) t); }
    MidiTiming getMidiTiming() const noexcept    { return (MidiTiming) midiTiming.load(); }

    void setOutputPair (int firstChannel) noexcept { outputPair.store (juce::jmax (0, firstChannel)); }
    int getOutputPair() const noexcept             { return outputPair.load(); }

    // ---- Mixer (F5): só afeta o que você ouve; a gravação é sempre antes do fader ----
    void setTrackGain (int track, float gain) noexcept { tracks[track].gain.store (gain); }
    void setTrackMute (int track, bool m) noexcept     { tracks[track].mute.store (m); }
    void setTrackSolo (int track, bool s) noexcept     { tracks[track].solo.store (s); }
    void setInputMonitor (int input, bool on) noexcept { tracks[trackInput1 + input].monitor.store (on); }
    void setInputChannel (int input, int ch) noexcept  { tracks[trackInput1 + input].channel.store (juce::jmax (0, ch)); }
    void setMasterGain (float g) noexcept              { masterGain.store (g); }

    // Medidores (pico desde a última leitura). Bateria/Chrome: depois do fader. Entradas: ANTES do fader
    // (é o nível que vai para a gravação). Clip: pico >= 0 dBFS desde a última leitura.
    float takeTrackPeak (int track) noexcept { return tracks[track].peak.exchange (0.0f); }
    bool takeTrackClip (int track) noexcept  { return tracks[track].clip.exchange (false); }
    float takeMasterPeakLeft() noexcept      { return masterPeakL.exchange (0.0f); }
    float takeMasterPeakRight() noexcept     { return masterPeakR.exchange (0.0f); }
    bool takeMasterClip() noexcept           { return masterClip.exchange (false); }
    int getNumInputChannels() const noexcept { return numDeviceInputs.load(); }

    // ---- Metrônomo (F6): clique sintetizado na thread de áudio, não vai para a gravação ----
    // A grade é o próprio transporte: o tempo 1 do compasso 1 cai na amostra 0 do take.
    // A contagem toca o clique mesmo com o metrônomo desligado.
    void setMetronomeOn (bool on) noexcept       { metronomeOn.store (on); }
    void setTempo (double bpm) noexcept          { tempoBpm.store (juce::jlimit (20.0, 400.0, bpm)); }
    void setBeatsPerBar (int n) noexcept         { beatsPerBar.store (juce::jlimit (1, 16, n)); }
    void setMetronomeGain (float g) noexcept     { metronomeGain.store (g); }
    double getTempo() const noexcept             { return tempoBpm.load(); }
    int getBeatsPerBar() const noexcept          { return beatsPerBar.load(); }
    bool isMetronomeAudible() const noexcept     { return metronomeOn.load() || getTransportState() == TransportState::countIn; }
    // Último tempo que passou (contado desde o início do take; negativo durante a contagem).
    std::int64_t getMetronomeBeat() const noexcept { return metronomeBeat.load(); }

    // Para todas as notas (CC123 em todos os canais).
    void panic();

    std::uint32_t getDroppedMidiCount() const noexcept { return droppedMidi.load(); }
    std::uint32_t getSkippedBlockCount() const noexcept { return skippedBlocks.load(); }

    double getSampleRate() const noexcept { return sampleRate.load(); }
    int getBlockSize() const noexcept     { return blockSize.load(); }

    // ---- thread MIDI (único produtor desta fila) ----
    void pushMidi (const MidiEvent& e) noexcept
    {
        if (! midiQueue.push (e))
            droppedMidi.fetch_add (1, std::memory_order_relaxed);
    }

    // ---- thread de mensagens (teclado do PC, botão "testar"): fila separada, também SPSC ----
    void pushUiMidi (const MidiEvent& e) noexcept
    {
        if (! uiMidiQueue.push (e))
            droppedMidi.fetch_add (1, std::memory_order_relaxed);
    }
    void playNote (int note, int velocity, bool on) noexcept;

    // ---- Transporte (pedidos da thread de mensagens; a thread de áudio aplica no começo do próximo bloco) ----
    //
    // Gravar: `drumWriter` recebe a saída do plugin (antes do volume) e as notas que o plugin recebeu saem
    // por popRecordedMidi() com a posição em amostras desde o início da gravação. Áudio e MIDI usam o mesmo
    // contador de amostras do callback, então ficam alinhados amostra a amostra.
    // `skipAudioSamples` = latência do plugin: descarta o começo do áudio para compensar.
    //
    // Os recursos (writer, eventos) pertencem a quem chamou e só podem ser liberados depois que
    // takeResourcesReleased() devolver true.
    // Chrome: grava o que saiu do resampler, já no relógio do ASIO. Entradas: gravadas adiantadas em
    // `inputCompensation` amostras (o restante do fim é completado com silêncio por quem gravou).
    bool requestRecord (const RecordTargets& targets) noexcept;
    bool requestPlay (const TimedMidi* events, int numEvents) noexcept;
    void requestStop() noexcept;

    TransportState getTransportState() const noexcept { return (TransportState) transportState.load(); }
    bool takeResourcesReleased() noexcept             { return resourcesReleased.exchange (false); }
    std::uint64_t getTransportPosition() const noexcept { return transportPosition.load(); }

    bool popRecordedMidi (TimedMidi& e) noexcept { return recordedMidi.pop (e); }
    std::uint32_t getRecordedMidiDropped() const noexcept { return recordedMidiDropped.load(); }

    // ---- Chrome (F4) ----
    // Troca o fluxo capturado (nulo = desligado). Thread de mensagens; ao voltar, a thread de áudio
    // garantidamente não usa mais o fluxo anterior, que pode então ser destruído.
    void setChromeStream (LoopbackStream* stream) noexcept;
    const DriftResampler& getChromeResampler() const noexcept { return chromeResampler; }

    // ---- AudioIODeviceCallback ----
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    void preparePlugin (juce::AudioPluginInstance& p, double sr, int bs);
    void allocateBuffers (int bs);
    void collectMidi (int numSamples, double blockStartSec);
    void renderChunk (int startSample, int numSamples);
    void computeTrackGains() noexcept;
    void processInputs (const float* const* inputs, int numInputs, int numSamples) noexcept;
    void processMaster (float* const* outputs, int numOutputs, int numSamples) noexcept;
    void renderMetronome (int numSamples) noexcept;
    void beginRecording() noexcept;
    static void storePeak (std::atomic<float>& target, float value) noexcept;

    // Transporte (thread de áudio)
    void applyTransportRequest (int numSamples);
    void addPlaybackEvents (int numSamples);
    void recordDrumAudio (const float* left, const float* right, int numSamples) noexcept;
    void recordDrumSilence (int numSamples) noexcept;
    void captureRecordedMidi (int chunkStart);
    void endTransport (bool sendNotesOff);
    void processChrome (int numSamples) noexcept;

    juce::SpinLock pluginLock;
    std::unique_ptr<juce::AudioPluginInstance> plugin;

    SpscQueue<MidiEvent, 1024> midiQueue;
    SpscQueue<MidiEvent, 256> uiMidiQueue;
    juce::MidiBuffer midiBuffer, chunkMidi;
    std::atomic<bool> panicRequested { false };
    juce::AudioBuffer<float> pluginBuffer;
    int pluginBufferChannels = 2;

    std::atomic<double> sampleRate { 0.0 };
    std::atomic<int> blockSize { 0 };
    std::atomic<bool> deviceRunning { false };

    std::atomic<int> midiTiming { (int) MidiTiming::lowestLatency };
    std::atomic<int> outputPair { 0 };
    std::atomic<std::uint32_t> droppedMidi { 0 }, skippedBlocks { 0 };

    // ---- mixer ----
    struct TrackControl
    {
        std::atomic<float> gain { 1.0f };
        std::atomic<bool> mute { false }, solo { false }, monitor { false };
        std::atomic<int> channel { 0 };       // entradas: índice entre os canais de entrada ativos
        std::atomic<float> peak { 0.0f };
        std::atomic<bool> clip { false };
    };
    TrackControl tracks[numTracks];
    std::atomic<float> masterGain { 1.0f };
    std::atomic<float> masterPeakL { 0.0f }, masterPeakR { 0.0f };
    std::atomic<bool> masterClip { false };
    std::atomic<int> numDeviceInputs { 0 };
    float blockGains[numTracks] {};          // só a thread de áudio (calculado no começo do bloco)
    juce::AudioBuffer<float> mixBus;         // soma de todas as faixas antes do master

    // ---- transporte ----
    enum Request { reqNone = 0, reqRecord, reqPlay, reqStop };
    std::atomic<int> pendingRequest { reqNone };
    std::atomic<int> transportState { (int) TransportState::idle };
    std::atomic<bool> resourcesReleased { false };
    std::atomic<std::uint64_t> transportPosition { 0 };

    // Preenchidos pela thread de mensagens ANTES de publicar o pedido (release/acquire em pendingRequest).
    RecordTargets requestedTargets;
    const TimedMidi* requestedEvents = nullptr;
    int requestedNumEvents = 0;

    // Só a thread de áudio mexe nestes.
    std::uint64_t sampleCounter = 0;      // amostras desde que o dispositivo abriu
    std::uint64_t transportStart = 0;     // sampleCounter no início da gravação/reprodução
    DiskWriter* activeWriter = nullptr;
    DiskWriter* activeChromeWriter = nullptr;
    DiskWriter* activeInputWriters[numInputTracks] {};
    int audioSkipRemaining = 0;
    int chromeSkipRemaining = 0;   // mesma compensação da bateria: as duas faixas alinhadas "como ouvido"
    int inputSkipRemaining[numInputTracks] {};

    // ---- Chrome ----
    std::atomic<LoopbackStream*> chromeStream { nullptr };
    std::atomic<bool> chromeInUse { false };
    DriftResampler chromeResampler;
    juce::AudioBuffer<float> chromeBuffer;
    const TimedMidi* playEvents = nullptr;
    int playNumEvents = 0, playIndex = 0;
    bool notesOffRequested = false;

    // ---- metrônomo ----
    std::atomic<bool> metronomeOn { false };
    std::atomic<double> tempoBpm { 120.0 };
    std::atomic<int> beatsPerBar { 4 };
    std::atomic<float> metronomeGain { 0.5f };
    std::atomic<std::int64_t> metronomeBeat { 0 };
    juce::AudioBuffer<float> clickAccent, clickBeat; // alocados em audioDeviceAboutToStart
    // Só a thread de áudio:
    std::int64_t metroPos = 0;          // amostras desde o tempo 0 (negativo durante a contagem)
    double metroSamplesPerBeat = 0.0;
    bool metroWasAudible = false;
    const float* clickData = nullptr;
    int clickPos = 0, clickLength = 0;
    std::int64_t countInRemaining = 0;

    SpscQueue<TimedMidi, 8192> recordedMidi;
    std::atomic<std::uint32_t> recordedMidiDropped { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace bb
