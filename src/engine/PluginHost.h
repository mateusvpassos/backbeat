#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

namespace bb
{

// Carrega plugins VST3 e salva/restaura o estado deles. Só thread de mensagens.
class PluginHost
{
public:
    PluginHost();

    static juce::File defaultDrumPluginFile(); // C:\Program Files\Common Files\VST3\SSDSampler5.vst3

    struct LoadResult
    {
        std::unique_ptr<juce::AudioPluginInstance> instance;
        juce::String error;
    };

    // Instancia o primeiro plugin (instrumento, de preferência) encontrado no arquivo .vst3.
    LoadResult loadVst3 (const juce::File& file, double sampleRate, int blockSize);

    // Estado do plugin (kit, mixer interno etc.) em base64, para guardar no JSON.
    static juce::String getStateBase64 (juce::AudioPluginInstance& plugin);
    static bool setStateBase64 (juce::AudioPluginInstance& plugin, const juce::String& base64);

private:
    static void useStereoMainOutput (juce::AudioPluginInstance& plugin);

    juce::AudioPluginFormatManager formatManager;
};

} // namespace bb
