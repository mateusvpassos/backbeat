#pragma once

#include <juce_core/juce_core.h>

#include <optional>

namespace bb
{

// Peças (articulações) que o plugin de bateria entende, com a nota MIDI de cada uma.
struct DrumPiece
{
    int note = 0;
    juce::String name;   // nome do SSD5 (igual ao que aparece na interface dele), ex.: "Hi-Hat Tip Closed eD"
    juce::String group;  // grupo em português, ex.: "Chimbal"
};

class DrumCatalog
{
public:
    // Kit "Deluxe 2 Free Edition" do SSD5 (lido do .ssd5kit em 09/11/2026). Usado se o arquivo não existir.
    static DrumCatalog builtInSsd5();

    // Lê as articulações de um arquivo .ssd5kit (usa a nota remapeada, a que o SSD5 responde de fato).
    static std::optional<DrumCatalog> fromSsd5KitText (const juce::String& text);
    static std::optional<DrumCatalog> fromSsd5KitFile (const juce::File& file);

    static juce::File defaultSsd5KitFile();

    // Peças do General MIDI (canal 10), para plugins que não são o SSD5.
    static DrumCatalog builtInGm();
    bool isSsd5 = true; // false = tabela GM (esconde o botão "Kit do SSD5...")

    const juce::Array<DrumPiece>& getPieces() const noexcept { return pieces; }
    const DrumPiece* find (int note) const noexcept;
    juce::String nameFor (int note) const; // "Hi-Hat Tip Closed eD", ou "nota 99" se não existir
    juce::StringArray groupsInOrder() const;

    juce::String sourceDescription; // "kit: Deluxe 2 Free Edition" ou "tabela embutida"

private:
    void add (int note, const juce::String& name, const juce::String& group);
    void sortByGroup();

    juce::Array<DrumPiece> pieces;
};

} // namespace bb
