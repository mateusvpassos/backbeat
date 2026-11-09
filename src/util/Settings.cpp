#include "util/Settings.h"

namespace bb
{

Settings::Settings (juce::File dataDirectory)
    : dataDir (std::move (dataDirectory)),
      root (new juce::DynamicObject())
{
}

juce::File Settings::defaultDataDirectory()
{
    const auto overridePath = juce::SystemStats::getEnvironmentVariable ("BACKBEAT_DATA", {});

    if (overridePath.isNotEmpty())
        return juce::File (overridePath);

    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Backbeat");
}

bool Settings::load()
{
    root = juce::var (new juce::DynamicObject());

    const auto file = getFile();
    if (! file.existsAsFile())
        return false;

    auto parsed = juce::JSON::parse (file.loadFileAsString());
    if (! parsed.isObject())
        return false;

    root = parsed;
    return true;
}

bool Settings::save() const
{
    if (! dataDir.createDirectory())
        return false;

    if (auto* obj = root.getDynamicObject())
        obj->setProperty ("versao", currentVersion);

    // replaceWithText escreve num arquivo temporário e troca no fim: não corrompe se o app cair.
    return getFile().replaceWithText (juce::JSON::toString (root, false), false, false, "\n");
}

juce::DynamicObject* Settings::sectionObject (const juce::String& section, bool create)
{
    auto* obj = root.getDynamicObject();
    if (obj == nullptr)
        return nullptr;

    auto existing = obj->getProperty (section);
    if (auto* sec = existing.getDynamicObject())
        return sec;

    if (! create)
        return nullptr;

    auto* sec = new juce::DynamicObject();
    obj->setProperty (section, juce::var (sec));
    return sec;
}

juce::var Settings::get (const juce::String& section, const juce::String& key, const juce::var& fallback) const
{
    if (auto* obj = root.getDynamicObject())
        if (auto* sec = obj->getProperty (section).getDynamicObject())
            if (sec->hasProperty (key))
                return sec->getProperty (key);

    return fallback;
}

void Settings::set (const juce::String& section, const juce::String& key, const juce::var& value)
{
    if (auto* sec = sectionObject (section, true))
        sec->setProperty (key, value);
}

} // namespace bb
