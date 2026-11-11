#include "ui/PluginWindow.h"
#include "ui/WindowChrome.h"

namespace bb
{

PluginWindow::PluginWindow (juce::AudioProcessor& p)
    : juce::DocumentWindow (p.getName(), juce::Colours::black, juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);

    juce::AudioProcessorEditor* editor = p.hasEditor() ? p.createEditorAndMakeActive() : nullptr;
    if (editor == nullptr)
        editor = new juce::GenericAudioProcessorEditor (p);

    setContentOwned (editor, true);
    setResizable (editor->isResizable(), false);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
    darkTitleBar (*this);
}

PluginWindow::~PluginWindow()
{
    clearContentComponent(); // destrói o editor enquanto o plugin ainda existe
}

void PluginWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}

} // namespace bb
