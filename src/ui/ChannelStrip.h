#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/LevelMeter.h"

#include <functional>

namespace bb
{

// Tira do mixer (estilo .strip da família): ponto colorido, nome, legenda, fader vertical em dB com
// VU ao lado, e botões M (mudo) / S (solo); nas entradas também ● (gravar) e Ouvir.
class ChannelStrip final : public juce::Component
{
public:
    struct Config
    {
        juce::String name;
        juce::Colour colour;
        bool stereo = true;          // dois VUs (L/R) ou um
        bool muteSolo = true;
        bool armAndMonitor = false;  // entradas
    };

    explicit ChannelStrip (Config);

    std::function<void (double gainDb)> onGain;   // durante o arraste
    std::function<void()> onGainCommitted;        // ao soltar (para salvar)
    std::function<void (bool)> onMute, onSolo, onArm, onMonitor;

    void setGainDb (double db);
    double getGainDb() const { return fader.getValue(); }
    void setMute (bool m)    { mute.setToggleState (m, juce::dontSendNotification); repaint(); }
    void setSolo (bool s)    { solo.setToggleState (s, juce::dontSendNotification); repaint(); }
    void setArm (bool a)     { arm.setToggleState (a, juce::dontSendNotification); repaint(); }
    void setMonitor (bool m) { monitor.setToggleState (m, juce::dontSendNotification); repaint(); }
    bool isArmed() const     { return arm.getToggleState(); }
    bool isMuted() const     { return mute.getToggleState(); }
    bool isSoloed() const    { return solo.getToggleState(); }
    void setCaption (const juce::String& text) { caption.setText (text, juce::dontSendNotification); }
    juce::String getName() const { return config.name; }

    void setLevels (float left, float right, bool clipped);

    void paint (juce::Graphics&) override;
    void resized() override;

    static double gainToDb (float gain);

private:
    Config config;
    juce::Label nameLabel, caption;
    juce::Slider fader;
    LevelMeter meterL, meterR;
    juce::TextButton mute { "M" }, solo { "S" }, arm { juce::String::fromUTF8 ("\xe2\x97\x8f Gravar") }, monitor { "Ouvir" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};

} // namespace bb
