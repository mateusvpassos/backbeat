#include "rec/DiskWriter.h"

namespace bb
{

std::unique_ptr<DiskWriter> DiskWriter::createWav (const juce::File& file, double sampleRate, int numChannels,
                                                   int bitsPerSample, double bufferSeconds, juce::String& error)
{
    if (! file.getParentDirectory().createDirectory())
    {
        error = L"Não foi possível criar a pasta " + file.getParentDirectory().getFullPathName();
        return nullptr;
    }

    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);

    if (auto* fos = dynamic_cast<juce::FileOutputStream*> (stream.get()); fos == nullptr || ! fos->openedOk())
    {
        error = L"Não foi possível criar " + file.getFullPathName();
        return nullptr;
    }

    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions()
                                                   .withSampleRate (sampleRate)
                                                   .withNumChannels (numChannels)
                                                   .withBitsPerSample (bitsPerSample));
    if (writer == nullptr)
    {
        error = "Formato WAV recusou " + juce::String (bitsPerSample) + " bits / " + juce::String (numChannels) + " canais";
        return nullptr;
    }

    const int capacity = juce::jmax (8192, (int) (sampleRate * bufferSeconds));
    auto dw = std::unique_ptr<DiskWriter> (new DiskWriter (file, std::move (writer), numChannels, capacity));
    dw->startThread (juce::Thread::Priority::high);
    return dw;
}

DiskWriter::DiskWriter (const juce::File& f, std::unique_ptr<juce::AudioFormatWriter> w, int channels, int capacity)
    : juce::Thread ("Backbeat disco: " + f.getFileName()),
      file (f),
      writer (std::move (w)),
      numChannels (channels),
      fifo (capacity),
      ring (channels, capacity)
{
    ring.clear();
}

DiskWriter::~DiskWriter()
{
    finish();
}

bool DiskWriter::write (const float* const* channels, int numChannelsIn, int numSamples) noexcept
{
    if (numSamples <= 0)
        return true;

    if (fifo.getFreeSpace() < numSamples)
    {
        droppedSamples.fetch_add ((std::uint64_t) numSamples, std::memory_order_relaxed);
        return false;
    }

    const auto scope = fifo.write (numSamples);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* src = numChannelsIn > 0 ? channels[juce::jmin (ch, numChannelsIn - 1)] : nullptr;

        if (src == nullptr)
        {
            if (scope.blockSize1 > 0) ring.clear (ch, scope.startIndex1, scope.blockSize1);
            if (scope.blockSize2 > 0) ring.clear (ch, scope.startIndex2, scope.blockSize2);
            continue;
        }

        if (scope.blockSize1 > 0) ring.copyFrom (ch, scope.startIndex1, src, scope.blockSize1);
        if (scope.blockSize2 > 0) ring.copyFrom (ch, scope.startIndex2, src + scope.blockSize1, scope.blockSize2);
    }

    return true;
}

bool DiskWriter::writeSilence (int numSamples) noexcept
{
    if (numSamples <= 0)
        return true;

    if (fifo.getFreeSpace() < numSamples)
    {
        droppedSamples.fetch_add ((std::uint64_t) numSamples, std::memory_order_relaxed);
        return false;
    }

    const auto scope = fifo.write (numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (scope.blockSize1 > 0) ring.clear (ch, scope.startIndex1, scope.blockSize1);
        if (scope.blockSize2 > 0) ring.clear (ch, scope.startIndex2, scope.blockSize2);
    }
    return true;
}

float DiskWriter::getFillLevel() const noexcept
{
    return (float) fifo.getNumReady() / (float) juce::jmax (1, fifo.getTotalSize() - 1);
}

void DiskWriter::drain()
{
    for (;;)
    {
        const int ready = fifo.getNumReady();
        if (ready <= 0)
            return;

        const auto scope = fifo.read (ready);
        if (scope.blockSize1 > 0) writer->writeFromAudioSampleBuffer (ring, scope.startIndex1, scope.blockSize1);
        if (scope.blockSize2 > 0) writer->writeFromAudioSampleBuffer (ring, scope.startIndex2, scope.blockSize2);
        writtenSamples.fetch_add ((std::uint64_t) (scope.blockSize1 + scope.blockSize2), std::memory_order_relaxed);
    }
}

void DiskWriter::run()
{
    while (! threadShouldExit())
    {
        drain();
        wait (5); // acorda sozinha; a thread de áudio nunca sinaliza
    }
}

void DiskWriter::finish()
{
    if (finished)
        return;

    finished = true;
    stopThread (5000);
    drain();        // o que chegou depois da última volta da thread
    writer.reset(); // fecha o arquivo e corrige o cabeçalho WAV
}

} // namespace bb
