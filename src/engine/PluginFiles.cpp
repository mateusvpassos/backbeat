#include "engine/PluginHost.h"

// Busca de arquivos .vst3 (sem abrir plugins): compilada também nos testes, que não têm suporte a VST3.

namespace bb
{

juce::File PluginHost::defaultDrumPluginFile()
{
    return juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory) // C:\Program Files
        .getChildFile ("Common Files").getChildFile ("VST3").getChildFile ("SSDSampler5.vst3");
}

namespace
{
    void collectVst3 (const juce::File& folder, juce::Array<juce::File>& out, int depth)
    {
        if (depth > 5)
            return;

        for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*",
                                                                juce::File::findFilesAndDirectories))
        {
            const auto f = entry.getFile();
            if (f.hasFileExtension ("vst3"))
                out.add (f); // um .vst3 pode ser pasta (bundle): não entra nela
            else if (entry.isDirectory())
                collectVst3 (f, out, depth + 1);
        }
    }
}

juce::Array<juce::File> PluginHost::findVst3In (const juce::File& folder)
{
    juce::Array<juce::File> out;
    if (folder.isDirectory())
        collectVst3 (folder, out, 0);
    return out;
}

juce::Array<juce::File> PluginHost::defaultVst3Folders()
{
    return {
        defaultDrumPluginFile().getParentDirectory(), // C:\Program Files\Common Files\VST3
        juce::File::getSpecialLocation (juce::File::windowsLocalAppData)
            .getChildFile ("Programs").getChildFile ("Common").getChildFile ("VST3"),
    };
}

juce::Array<juce::File> PluginHost::findInstalledVst3 (const juce::StringArray& extraFolders)
{
    auto folders = defaultVst3Folders();
    for (const auto& path : extraFolders)
        if (juce::File::isAbsolutePath (path))
            folders.addIfNotAlreadyThere (juce::File (path));

    juce::Array<juce::File> all;
    for (const auto& folder : folders)
        for (const auto& f : findVst3In (folder))
            all.addIfNotAlreadyThere (f);

    std::sort (all.begin(), all.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()) < 0;
    });
    return all;
}

bool PluginHost::isSsd5 (const juce::File& pluginFile)
{
    return pluginFile.getFileNameWithoutExtension().startsWithIgnoreCase ("SSDSampler");
}

} // namespace bb
