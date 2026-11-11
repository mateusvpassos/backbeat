#include "ui/MappingWindow.h"
#include "ui/WindowChrome.h"

namespace bb
{

class MappingWindow::Content final : public juce::Component
{
public:
    explicit Content (MappingContext& ctx)
        : tabs (juce::TabbedButtonBar::TabsAtTop), pads (ctx), keys (ctx)
    {
        tabs.setTabBarDepth (34);
        tabs.setOutline (0);
        tabs.addTab ("Pads", theme::background, &pads, false);
        tabs.addTab ("Teclado", theme::background, &keys, false);
        addAndMakeVisible (tabs);
        setSize (900, 640);
    }

    void resized() override { tabs.setBounds (getLocalBounds()); }

    juce::TabbedComponent tabs;
    PadsPage pads;
    KeysPage keys;
};

MappingWindow::MappingWindow (MappingContext& ctx)
    : juce::DocumentWindow ("Mapeamento", theme::background, juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    content = new Content (ctx);
    setContentOwned (content, true);
    setResizable (true, false);
    setResizeLimits (760, 520, 1600, 1400);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
    darkTitleBar (*this);
}

void MappingWindow::refresh()
{
    content->pads.refresh();
    content->keys.refresh();
}

void MappingWindow::handleMidi (const MidiEvent& e)
{
    content->pads.handleMidi (e);
}

void MappingWindow::setHotkeyFailures (const juce::StringArray& failed)
{
    content->keys.setHotkeyFailures (failed);
}

} // namespace bb
