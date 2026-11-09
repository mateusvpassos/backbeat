#pragma once

#include <juce_core/juce_core.h>

namespace bb
{

// Configurações em JSON, organizadas por seção:
//   { "versao": 1, "audio": {...}, "plugin": {...}, "midi": {...}, "saida": {...} }
// Arquivo: <pasta de dados>/settings.json. Só usar na thread de mensagens.
class Settings
{
public:
    explicit Settings (juce::File dataDirectory);

    // %APPDATA%\Backbeat, ou a pasta em BACKBEAT_DATA (para testes, sem tocar nos dados reais).
    static juce::File defaultDataDirectory();

    bool load();             // false se o arquivo não existe ou é inválido (começa vazio)
    bool save() const;

    juce::var get (const juce::String& section, const juce::String& key, const juce::var& fallback = {}) const;
    void set (const juce::String& section, const juce::String& key, const juce::var& value);

    juce::File getDataDirectory() const { return dataDir; }
    juce::File getFile() const { return dataDir.getChildFile ("settings.json"); }

    static constexpr int currentVersion = 1;

private:
    juce::DynamicObject* sectionObject (const juce::String& section, bool create);

    juce::File dataDir;
    juce::var root;
};

} // namespace bb
