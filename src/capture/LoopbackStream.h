#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

namespace bb
{

// FIFO estéreo lock-free entre a thread de captura (WASAPI, produtora) e a thread de áudio (consumidora).
// Tamanho fixo, alocado na criação.
class LoopbackStream
{
public:
    explicit LoopbackStream (int capacityFrames)
        : fifo (capacityFrames), ring (2, capacityFrames)
    {
        ring.clear();
    }

    // ---- thread de captura ----
    void pushInterleaved (const float* data, int frames, int channels) noexcept
    {
        if (frames <= 0)
            return;

        if (fifo.getFreeSpace() < frames)
        {
            overflowFrames.fetch_add ((std::uint64_t) frames, std::memory_order_relaxed);
            return;
        }

        const auto scope = fifo.write (frames);
        const auto copy = [&] (int start, int count, int offset)
        {
            float* l = ring.getWritePointer (0, start);
            float* r = ring.getWritePointer (1, start);
            for (int i = 0; i < count; ++i)
            {
                const float* frame = data + (size_t) (offset + i) * (size_t) channels;
                l[i] = frame[0];
                r[i] = channels > 1 ? frame[1] : frame[0];
            }
        };
        if (scope.blockSize1 > 0) copy (scope.startIndex1, scope.blockSize1, 0);
        if (scope.blockSize2 > 0) copy (scope.startIndex2, scope.blockSize2, scope.blockSize1);
        pushedFrames.fetch_add ((std::uint64_t) frames, std::memory_order_relaxed);
    }

    void pushSilence (int frames) noexcept
    {
        if (frames <= 0)
            return;

        if (fifo.getFreeSpace() < frames)
        {
            overflowFrames.fetch_add ((std::uint64_t) frames, std::memory_order_relaxed);
            return;
        }

        const auto scope = fifo.write (frames);
        for (int ch = 0; ch < 2; ++ch)
        {
            if (scope.blockSize1 > 0) ring.clear (ch, scope.startIndex1, scope.blockSize1);
            if (scope.blockSize2 > 0) ring.clear (ch, scope.startIndex2, scope.blockSize2);
        }
        pushedFrames.fetch_add ((std::uint64_t) frames, std::memory_order_relaxed);
    }

    // ---- thread de áudio ----
    int getNumReady() const noexcept { return fifo.getNumReady(); }

    // Copia os primeiros `frames` sem consumir. Devolve false se não houver tudo isso.
    bool peek (float* left, float* right, int frames) const noexcept
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (frames, start1, size1, start2, size2);
        if (size1 + size2 < frames)
            return false;

        juce::FloatVectorOperations::copy (left, ring.getReadPointer (0, start1), size1);
        juce::FloatVectorOperations::copy (right, ring.getReadPointer (1, start1), size1);
        if (size2 > 0)
        {
            juce::FloatVectorOperations::copy (left + size1, ring.getReadPointer (0, start2), size2);
            juce::FloatVectorOperations::copy (right + size1, ring.getReadPointer (1, start2), size2);
        }
        return true;
    }

    void consume (int frames) noexcept { fifo.finishedRead (juce::jmin (frames, fifo.getNumReady())); }

    std::atomic<std::uint64_t> pushedFrames { 0 }, overflowFrames { 0 };

private:
    juce::AbstractFifo fifo;
    juce::AudioBuffer<float> ring;
};

} // namespace bb
