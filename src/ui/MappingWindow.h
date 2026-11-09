#pragma once

#include "ui/KeysPage.h"
#include "ui/PadsPage.h"

namespace bb
{

// Janela "Mapeamento" com as abas Pads e Teclado.
class MappingWindow final : public juce::DocumentWindow
{
public:
    explicit MappingWindow (MappingContext&);

    void refresh();
    void handleMidi (const MidiEvent& e);
    void setHotkeyFailures (const juce::StringArray& failed);

    std::function<void()> onClose;
    void closeButtonPressed() override { if (onClose) onClose(); }

private:
    class Content;
    Content* content = nullptr; // pertence à janela

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MappingWindow)
};

} // namespace bb
