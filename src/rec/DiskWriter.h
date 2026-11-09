#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <memory>

namespace bb
{

// Grava áudio em WAV sem bloquear a thread de áudio.
//   - write()/writeSilence(): thread de áudio. Só copiam para um FIFO lock-free (AbstractFifo) pré-alocado;
//     não alocam, não travam e não acordam ninguém (nada de notify/condition variable).
//   - Uma thread própria acorda a cada poucos ms, esvazia o FIFO e grava no arquivo.
//   - finish(): thread de mensagens. Grava o que sobrou e fecha o arquivo (cabeçalho WAV correto).
class DiskWriter final : private juce::Thread
{
public:
    static std::unique_ptr<DiskWriter> createWav (const juce::File& file, double sampleRate, int numChannels,
                                                  int bitsPerSample, double bufferSeconds, juce::String& error);
    ~DiskWriter() override;

    // ---- thread de áudio ----
    // Canais além de numChannels são ignorados; se vierem menos, o resto é preenchido com o último canal.
    bool write (const float* const* channels, int numChannelsIn, int numSamples) noexcept;
    bool writeSilence (int numSamples) noexcept;

    // ---- qualquer thread (leitura) ----
    float getFillLevel() const noexcept;                         // 0..1: quanto do FIFO está ocupado
    std::uint64_t getDroppedSamples() const noexcept { return droppedSamples.load(); }
    std::uint64_t getWrittenSamples() const noexcept { return writtenSamples.load(); }
    int getNumChannels() const noexcept { return numChannels; }
    const juce::File& getFile() const noexcept { return file; }

    // ---- thread de mensagens ----
    void finish(); // chame só depois que a thread de áudio parou de escrever

private:
    DiskWriter (const juce::File&, std::unique_ptr<juce::AudioFormatWriter>, int numChannels, int capacity);

    void run() override;
    void drain();

    juce::File file;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    const int numChannels;

    juce::AbstractFifo fifo;
    juce::AudioBuffer<float> ring;

    std::atomic<std::uint64_t> droppedSamples { 0 }, writtenSamples { 0 };
    bool finished = false;
};

} // namespace bb
