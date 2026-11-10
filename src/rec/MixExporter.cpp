#include "rec/MixExporter.h"

#include <memory>
#include <vector>

namespace bb::mixexport
{

juce::String trackFileName (int track)
{
    switch (track)
    {
        case trackDrums:  return "bateria.wav";
        case trackChrome: return "chrome.wav";
        case trackInput1: return "entrada-1.wav";
        case trackInput2: return "entrada-2.wav";
        default:          return {};
    }
}

namespace
{
    struct Source
    {
        std::unique_ptr<juce::AudioFormatReader> reader;
        float gain = 1.0f;
    };

    constexpr int chunk = 16384;
    constexpr float clipThreshold = 0.999f;
    constexpr float targetPeak = 0.8913f; // -1 dBFS

    // Soma um trecho de todas as faixas em `mix` (estéreo). Faixas mono vão para os dois lados.
    void mixChunk (std::vector<Source>& sources, juce::AudioBuffer<float>& mix, juce::AudioBuffer<float>& temp,
                   juce::int64 start, int len, float master)
    {
        mix.clear (0, len);
        for (auto& s : sources)
        {
            const int ch = (int) s.reader->numChannels;
            temp.clear();
            s.reader->read (&temp, 0, len, start, true, ch > 1); // além do fim: zeros
            const float g = s.gain * master;
            mix.addFrom (0, 0, temp, 0, 0, len, g);
            mix.addFrom (1, 0, temp, ch > 1 ? 1 : 0, 0, len, g);
        }
    }
}

Result exportTake (const juce::File& takeFolder, const Gains& gains,
                   const std::function<bool (float)>& progress, const juce::String& outputName)
{
    Result result;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::vector<Source> sources;
    double sampleRate = 0.0;
    juce::int64 length = 0;
    bool anyFile = false;

    for (int t = 0; t < numTracks; ++t)
    {
        const auto file = takeFolder.getChildFile (trackFileName (t));
        if (! file.existsAsFile())
            continue;
        anyFile = true;
        if (gains.track[t] <= 0.0f)
            continue; // muda/fora do solo: não entra

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr)
        {
            result.error = L"Não foi possível ler " + file.getFileName() + ".";
            return result;
        }
        if (sampleRate == 0.0)
            sampleRate = reader->sampleRate;
        else if (std::abs (reader->sampleRate - sampleRate) > 0.5)
        {
            result.error = file.getFileName() + L" tem outra taxa de amostragem.";
            return result;
        }

        length = juce::jmax (length, reader->lengthInSamples);
        sources.push_back ({ std::move (reader), gains.track[t] });
    }

    if (! anyFile)
    {
        result.error = L"Este take não tem faixas de áudio.";
        return result;
    }
    if (sources.empty() || gains.master <= 0.0f)
    {
        result.error = L"Todas as faixas estão mudas no mixer.";
        return result;
    }

    result.tracksUsed = (int) sources.size();
    result.seconds = (double) length / sampleRate;

    juce::AudioBuffer<float> mix (2, chunk), temp (2, chunk);

    // 1ª passada: pico da soma.
    float peak = 0.0f;
    for (juce::int64 pos = 0; pos < length; pos += chunk)
    {
        const int len = (int) juce::jmin ((juce::int64) chunk, length - pos);
        mixChunk (sources, mix, temp, pos, len, gains.master);
        peak = juce::jmax (peak, mix.getMagnitude (0, len));
        if (progress && ! progress (0.5f * (float) pos / (float) length))
        {
            result.cancelled = true;
            return result;
        }
    }

    result.peak = peak;
    result.appliedGain = peak > clipThreshold ? targetPeak / peak : 1.0f;

    // 2ª passada: grava num arquivo temporário e só troca no fim (uma mixagem antiga nunca fica pela metade).
    const auto finalFile = takeFolder.getChildFile (outputName);
    const auto tempFile = takeFolder.getChildFile (finalFile.getFileNameWithoutExtension() + ".tmp.wav");
    tempFile.deleteFile();

    {
        auto fileStream = std::make_unique<juce::FileOutputStream> (tempFile);
        if (! fileStream->openedOk())
        {
            result.error = L"Não foi possível criar " + tempFile.getFullPathName();
            return result;
        }

        std::unique_ptr<juce::OutputStream> stream (std::move (fileStream));
        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions()
                                                       .withSampleRate (sampleRate)
                                                       .withNumChannels (2)
                                                       .withBitsPerSample (24));
        if (writer == nullptr)
        {
            result.error = L"Não foi possível criar o WAV da mixagem.";
            return result;
        }

        for (juce::int64 pos = 0; pos < length; pos += chunk)
        {
            const int len = (int) juce::jmin ((juce::int64) chunk, length - pos);
            mixChunk (sources, mix, temp, pos, len, gains.master);
            if (result.appliedGain != 1.0f)
                mix.applyGain (0, len, result.appliedGain);

            if (! writer->writeFromAudioSampleBuffer (mix, 0, len))
            {
                writer.reset();
                tempFile.deleteFile();
                result.error = L"Erro ao gravar a mixagem (disco cheio?).";
                return result;
            }

            if (progress && ! progress (0.5f + 0.5f * (float) pos / (float) length))
            {
                writer.reset();
                tempFile.deleteFile();
                result.cancelled = true;
                return result;
            }
        }
    }

    finalFile.deleteFile();
    if (! tempFile.moveFileTo (finalFile))
    {
        result.error = L"Não foi possível substituir " + finalFile.getFullPathName() + L" (aberto em outro programa?)";
        return result;
    }

    if (progress)
        progress (1.0f);

    result.file = finalFile;
    result.ok = true;
    return result;
}

} // namespace bb::mixexport
