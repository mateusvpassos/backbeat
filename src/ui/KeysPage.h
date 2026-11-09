#pragma once

#include "ui/MappingCommon.h"

namespace bb
{

// Aba "Teclado": teclas do PC → peças do SSD5 ou comandos (gravar, tocar, parar, pânico).
class KeysPage final : public juce::Component,
                       private juce::ListBoxModel
{
public:
    explicit KeysPage (MappingContext&);
    ~KeysPage() override;

    void refresh();
    void setHotkeyFailures (const juce::StringArray& failed);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Row;
    class KeyCaptureButton;

    int getNumRows() override;
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int row, bool selected, juce::Component* existing) override;

    void commit();
    void assignKey (int row, const juce::KeyPress& key);

    MappingContext& ctx;
    juce::Label hintLabel, warningLabel;
    juce::ToggleButton globalToggle { "Comandos funcionam com outras janelas em foco (ex.: Songsterr no Chrome)" };
    juce::ListBox list;
    juce::TextButton addButton { "+ Adicionar tecla" };
    int captureRowOnRefresh = -1;
};

} // namespace bb
