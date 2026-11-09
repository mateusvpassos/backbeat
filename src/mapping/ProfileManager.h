#pragma once

#include <juce_core/juce_core.h>

#include "mapping/MappingProfile.h"

#include <functional>

namespace bb
{

// Perfis em <dados>/perfis/*.json. Um perfil fica ativo; editar e chamar commit() salva na hora.
// Só thread de mensagens.
class ProfileManager
{
public:
    explicit ProfileManager (juce::File dataDirectory);

    // Carrega o perfil pelo nome (ou o primeiro). Cria o perfil padrão se a pasta estiver vazia.
    void loadOrCreate (const juce::String& activeName);

    juce::StringArray names() const;
    const MappingProfile& current() const noexcept { return profile; }

    MappingProfile& edit() noexcept { return profile; } // altere e depois chame commit()
    void commit();                                      // salva + avisa

    bool select (const juce::String& name);
    void createNew (const juce::String& baseName, bool copyCurrent);
    bool rename (const juce::String& newName);
    bool removeCurrent(); // manda para a Lixeira; sempre sobra pelo menos um perfil

    juce::String uniqueName (const juce::String& base) const;
    juce::File getDirectory() const { return directory; }

    std::function<void()> onChanged; // perfil ativo trocou ou foi editado

private:
    juce::File fileFor (const juce::String& name) const;
    bool save (const MappingProfile&) const;
    static bool load (const juce::File&, MappingProfile& out);
    void notify() { if (onChanged) onChanged(); }

    juce::File directory;
    MappingProfile profile;
};

} // namespace bb
