#include "mapping/ProfileManager.h"

namespace bb
{

ProfileManager::ProfileManager (juce::File dataDirectory)
    : directory (dataDirectory.getChildFile ("perfis"))
{
}

juce::File ProfileManager::fileFor (const juce::String& name) const
{
    return directory.getChildFile (juce::File::createLegalFileName (name.trim()) + ".json");
}

bool ProfileManager::save (const MappingProfile& p) const
{
    if (! directory.createDirectory())
        return false;
    return fileFor (p.name).replaceWithText (juce::JSON::toString (p.toVar()), false, false, "\n");
}

bool ProfileManager::load (const juce::File& f, MappingProfile& out)
{
    const auto parsed = juce::JSON::parse (f.loadFileAsString());
    if (! parsed.isObject())
        return false;

    out = MappingProfile::fromVar (parsed);
    if (out.name.trim().isEmpty())
        out.name = f.getFileNameWithoutExtension();
    return true;
}

juce::StringArray ProfileManager::names() const
{
    juce::StringArray result;
    for (const auto& f : directory.findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        MappingProfile p;
        if (load (f, p))
            result.addIfNotAlreadyThere (p.name);
    }
    result.sortNatural();
    return result;
}

void ProfileManager::loadOrCreate (const juce::String& activeName)
{
    auto all = names();

    if (all.isEmpty())
    {
        save (MappingProfile::defaultNexusGm());
        all = names();
    }

    if (! select (activeName) && ! all.isEmpty())
        select (all[0]);
}

bool ProfileManager::select (const juce::String& name)
{
    if (name.isEmpty())
        return false;

    // Procura pelo nome dentro do arquivo (o nome do arquivo pode ter caracteres trocados).
    for (const auto& f : directory.findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        MappingProfile p;
        if (load (f, p) && p.name == name)
        {
            profile = p;
            notify();
            return true;
        }
    }
    return false;
}

void ProfileManager::commit()
{
    save (profile);
    notify();
}

juce::String ProfileManager::uniqueName (const juce::String& base) const
{
    const auto existing = names();
    auto candidate = base.trim().isEmpty() ? juce::String ("Perfil") : base.trim();

    for (int n = 2; existing.contains (candidate) || fileFor (candidate).exists(); ++n)
        candidate = base.trim() + " " + juce::String (n);

    return candidate;
}

void ProfileManager::createNew (const juce::String& baseName, bool copyCurrent)
{
    MappingProfile p = copyCurrent ? profile : MappingProfile();
    p.name = uniqueName (baseName);
    save (p);
    profile = p;
    notify();
}

bool ProfileManager::rename (const juce::String& newName)
{
    const auto trimmed = newName.trim();
    if (trimmed.isEmpty() || trimmed == profile.name)
        return false;

    if (names().contains (trimmed))
        return false;

    const auto oldFile = fileFor (profile.name);
    profile.name = trimmed;
    if (! save (profile))
        return false;

    if (oldFile != fileFor (trimmed))
        oldFile.deleteFile();

    notify();
    return true;
}

bool ProfileManager::removeCurrent()
{
    if (names().size() <= 1)
        return false;

    const auto f = fileFor (profile.name);
    if (! f.moveToTrash())
        f.deleteFile();

    const auto remaining = names();
    return ! remaining.isEmpty() && select (remaining[0]);
}

} // namespace bb
