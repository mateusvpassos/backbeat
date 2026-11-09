#include "engine/PluginHost.h"

namespace bb
{

PluginHost::PluginHost()
{
    juce::addDefaultFormatsToManager (formatManager);
}

juce::File PluginHost::defaultDrumPluginFile()
{
    return juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory) // C:\Program Files
        .getChildFile ("Common Files").getChildFile ("VST3").getChildFile ("SSDSampler5.vst3");
}

PluginHost::LoadResult PluginHost::loadVst3 (const juce::File& file, double sampleRate, int blockSize)
{
    LoadResult result;

    if (! file.exists())
    {
        result.error = L"Plugin não encontrado: " + file.getFullPathName();
        return result;
    }

    juce::VST3PluginFormat* vst3 = nullptr;
    for (auto* format : formatManager.getFormats())
        if (auto* f = dynamic_cast<juce::VST3PluginFormat*> (format))
            vst3 = f;

    if (vst3 == nullptr)
    {
        result.error = L"Suporte a VST3 não foi compilado.";
        return result;
    }

    juce::OwnedArray<juce::PluginDescription> types;
    vst3->findAllTypesForFile (types, file.getFullPathName());

    if (types.isEmpty())
    {
        result.error = L"Nenhum plugin VST3 válido em " + file.getFileName();
        return result;
    }

    const juce::PluginDescription* chosen = types.getFirst();
    for (auto* t : types)
        if (t->isInstrument)
        {
            chosen = t;
            break;
        }

    result.instance = formatManager.createPluginInstance (*chosen, sampleRate, blockSize, result.error);

    if (result.instance != nullptr)
        useStereoMainOutput (*result.instance);
    else if (result.error.isEmpty())
        result.error = "Falha ao abrir " + chosen->name;

    return result;
}

void PluginHost::useStereoMainOutput (juce::AudioPluginInstance& plugin)
{
    // O SSD5 oferece saídas extras (multi-out por peça). Por enquanto só a principal, em estéreo.
    auto layout = plugin.getBusesLayout();

    for (auto& bus : layout.inputBuses)
        bus = juce::AudioChannelSet::disabled();

    for (int i = 0; i < layout.outputBuses.size(); ++i)
        layout.outputBuses.getReference (i) = (i == 0) ? juce::AudioChannelSet::stereo()
                                                       : juce::AudioChannelSet::disabled();

    if (! plugin.setBusesLayout (layout))
    {
        // Se o plugin recusar, tenta só desligar as extras mantendo o formato da principal.
        auto fallback = plugin.getBusesLayout();
        for (int i = 1; i < fallback.outputBuses.size(); ++i)
            fallback.outputBuses.getReference (i) = juce::AudioChannelSet::disabled();
        plugin.setBusesLayout (fallback);
    }
}

juce::String PluginHost::getStateBase64 (juce::AudioPluginInstance& plugin)
{
    juce::MemoryBlock block;
    plugin.getStateInformation (block);
    return block.toBase64Encoding();
}

bool PluginHost::setStateBase64 (juce::AudioPluginInstance& plugin, const juce::String& base64)
{
    if (base64.isEmpty())
        return false;

    juce::MemoryBlock block;
    if (! block.fromBase64Encoding (base64) || block.getSize() == 0)
        return false;

    plugin.setStateInformation (block.getData(), (int) block.getSize());
    return true;
}

} // namespace bb
