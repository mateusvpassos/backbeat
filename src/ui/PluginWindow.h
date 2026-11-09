#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

namespace bb
{

// Janela com a interface gráfica do plugin. Precisa ser destruída ANTES do plugin.
class PluginWindow final : public juce::DocumentWindow
{
public:
    explicit PluginWindow (juce::AudioProcessor& processor);
    ~PluginWindow() override;

    std::function<void()> onClose;

    void closeButtonPressed() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};

} // namespace bb
