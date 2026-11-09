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
    playing
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

    void setDrumGain (float g) noexcept { drumGain.store (g); }
    float getDrumGain() const noexcept  { return drumGain.load(); }

    // Para todas as notas (CC123 em todos os canais).
    void panic();

    // Medidores: pico desde a última leitura (a UI chama e zera).
    float takePeakLeft() noexcept  { return peakLeft.exchange (0.0f); }
    float takePeakRight() noexcept { return peakRight.exchange (0.0f); }

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
    // `chromeWriter` pode ser nulo (Chrome desligado): grava o que saiu do resampler, já no relógio do ASIO.
    bool requestRecord (DiskWriter* drumWriter, DiskWriter* chromeWriter, int skipAudioSamples) noexcept;
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
    void setChromeGain (float g) noexcept   { chromeGain.store (g); }
    void setChromeMuted (bool m) noexcept   { chromeMuted.store (m); }
    float takeChromePeakLeft() noexcept     { return chromePeakL.exchange (0.0f); }
    float takeChromePeakRight() noexcept    { return chromePeakR.exchange (0.0f); }
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
    void renderChunk (float* const* outputs, int numOutputs, int startSample, int numSamples);

    // Transporte (thread de áudio)
    void applyTransportRequest();
    void addPlaybackEvents (int numSamples);
    void recordDrumAudio (const float* left, const float* right, int numSamples) noexcept;
    void recordDrumSilence (int numSamples) noexcept;
    void captureRecordedMidi (int chunkStart);
    void endTransport (bool sendNotesOff);
    void processChrome (float* const* outputs, int numOutputs, int numSamples) noexcept;

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
    std::atomic<float> drumGain { 1.0f };

    std::atomic<float> peakLeft { 0.0f }, peakRight { 0.0f };
    std::atomic<std::uint32_t> droppedMidi { 0 }, skippedBlocks { 0 };

    // ---- transporte ----
    enum Request { reqNone = 0, reqRecord, reqPlay, reqStop };
    std::atomic<int> pendingRequest { reqNone };
    std::atomic<int> transportState { (int) TransportState::idle };
    std::atomic<bool> resourcesReleased { false };
    std::atomic<std::uint64_t> transportPosition { 0 };

    // Preenchidos pela thread de mensagens ANTES de publicar o pedido (release/acquire em pendingRequest).
    DiskWriter* requestedWriter = nullptr;
    DiskWriter* requestedChromeWriter = nullptr;
    int requestedSkip = 0;
    const TimedMidi* requestedEvents = nullptr;
    int requestedNumEvents = 0;

    // Só a thread de áudio mexe nestes.
    std::uint64_t sampleCounter = 0;      // amostras desde que o dispositivo abriu
    std::uint64_t transportStart = 0;     // sampleCounter no início da gravação/reprodução
    DiskWriter* activeWriter = nullptr;
    DiskWriter* activeChromeWriter = nullptr;
    int audioSkipRemaining = 0;
    int chromeSkipRemaining = 0;   // mesma compensação da bateria: as duas faixas alinhadas "como ouvido"

    // ---- Chrome ----
    std::atomic<LoopbackStream*> chromeStream { nullptr };
    std::atomic<bool> chromeInUse { false };
    DriftResampler chromeResampler;
    juce::AudioBuffer<float> chromeBuffer;
    std::atomic<float> chromeGain { 1.0f };
    std::atomic<bool> chromeMuted { false };
    std::atomic<float> chromePeakL { 0.0f }, chromePeakR { 0.0f };
    const TimedMidi* playEvents = nullptr;
    int playNumEvents = 0, playIndex = 0;
    bool notesOffRequested = false;

    SpscQueue<TimedMidi, 8192> recordedMidi;
    std::atomic<std::uint32_t> recordedMidiDropped { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace bb
