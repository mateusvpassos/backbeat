#include "ui/ChannelStrip.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include "ui/Theme.h"

namespace bb
{

namespace
{
    constexpr double minDb = -60.0, maxDb = 6.0;

    void setOnColour (juce::Button& b, juce::Colour c)
    {
        b.getProperties().set ("onColour", (juce::int64) c.getARGB());
    }
}

double ChannelStrip::gainToDb (float gain)
{
    return juce::jlimit (minDb, maxDb, (double) juce::Decibels::gainToDecibels (gain, (float) minDb));
}

ChannelStrip::ChannelStrip (Config c) : config (std::move (c))
{
    nameLabel.setText (config.name, juce::dontSendNotification);
    nameLabel.setFont (theme::font (13.5f, theme::Weight::semibold));
    nameLabel.setColour (juce::Label::textColourId, theme::text);
    nameLabel.setMinimumHorizontalScale (0.8f);
    theme::caption (caption, {});
    caption.setFont (theme::font (11.5f));
    caption.setColour (juce::Label::textColourId, theme::dim);
    caption.setMinimumHorizontalScale (0.75f);

    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setRange (minDb, maxDb, 0.1);
    fader.setSkewFactorFromMidPoint (-12.0);   // 0 dB perto de 3/4 da altura, como numa mesa
    fader.setDoubleClickReturnValue (true, 0.0);
    fader.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 20);
    fader.setTextValueSuffix (" dB");
    fader.setColour (juce::Slider::trackColourId, config.colour);
    fader.getProperties().set ("unityPos", 0.0);
    fader.textFromValueFunction = [] (double v)
    {
        return v <= minDb + 0.05 ? juce::String (juce::CharPointer_UTF8 ("-\xe2\x88\x9e")) // -∞
                                 : juce::String (v, 1);
    };
    fader.onValueChange = [this] { if (onGain) onGain (fader.getValue()); };
    fader.onDragEnd = [this] { if (onGainCommitted) onGainCommitted(); };

    for (auto* m : { &meterL, &meterR })
    {
        m->setVertical (true);
        m->setBarColour (config.colour);
    }

    for (auto* b : { &mute, &solo, &arm, &monitor })
    {
        b->setClickingTogglesState (true);
        b->setWantsKeyboardFocus (false);
    }
    setOnColour (mute, theme::warn);
    setOnColour (solo, juce::Colour (0xff2dd4bf));
    setOnColour (arm, theme::danger);
    setOnColour (monitor, config.colour);

    mute.setTooltip ("Mudo (M)");
    solo.setTooltip (L"Solo: com alguma faixa em solo, só as faixas em solo tocam");
    arm.setTooltip (L"Gravar esta entrada na próxima gravação");
    monitor.setTooltip (L"Ouvir esta entrada pelo Backbeat (com atraso). Com o Direct Monitor da Behringer, deixe desligado.");

    mute.onClick    = [this] { repaint(); if (onMute) onMute (mute.getToggleState()); };
    solo.onClick    = [this] { repaint(); if (onSolo) onSolo (solo.getToggleState()); };
    arm.onClick     = [this] { repaint(); if (onArm) onArm (arm.getToggleState()); };
    monitor.onClick = [this] { repaint(); if (onMonitor) onMonitor (monitor.getToggleState()); };

    addAndMakeVisible (nameLabel);
    addAndMakeVisible (caption);
    addAndMakeVisible (fader);
    addAndMakeVisible (meterL);
    if (config.stereo)
        addAndMakeVisible (meterR);
    if (config.muteSolo)
    {
        addAndMakeVisible (mute);
        addAndMakeVisible (solo);
    }
    if (config.armAndMonitor)
    {
        addAndMakeVisible (arm);
        addAndMakeVisible (monitor);
    }
}

void ChannelStrip::setGainDb (double db)
{
    fader.setValue (juce::jlimit (minDb, maxDb, db), juce::dontSendNotification);
}

void ChannelStrip::setLevels (float left, float right, bool clipped)
{
    meterL.setPeak (left);
    meterL.setClipped (clipped);
    if (config.stereo)
    {
        meterR.setPeak (right);
        meterR.setClipped (clipped);
    }
}

void ChannelStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool active = monitor.getToggleState() || arm.getToggleState() || solo.getToggleState();

    g.setColour (theme::panel2);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (active ? config.colour.withAlpha (0.35f).overlaidWith (theme::border.withAlpha (0.4f)) : theme::border);
    g.drawRoundedRectangle (r, 10.0f, 1.0f);

    // .st-dot: ponto da cor da faixa (verde-claro se está gravando-armado)
    const auto dot = juce::Rectangle<float> (12.0f, 15.0f, 8.0f, 8.0f);
    g.setColour (mute.getToggleState() ? theme::dim : (arm.getToggleState() ? theme::danger : config.colour));
    g.fillEllipse (dot);
}

void ChannelStrip::resized()
{
    auto r = getLocalBounds().reduced (10, 8);

    auto top = r.removeFromTop (22);
    top.removeFromLeft (16); // ponto
    nameLabel.setBounds (top);
    caption.setBounds (r.removeFromTop (16).withTrimmedLeft (2));
    r.removeFromTop (6);

    if (config.armAndMonitor)
    {
        auto row = r.removeFromBottom (24);
        arm.setBounds (row.removeFromLeft ((row.getWidth() - 6) / 2));
        row.removeFromLeft (6);
        monitor.setBounds (row);
        r.removeFromBottom (6);
    }

    if (config.muteSolo)
    {
        auto row = r.removeFromBottom (24);
        mute.setBounds (row.removeFromLeft ((row.getWidth() - 6) / 2));
        row.removeFromLeft (6);
        solo.setBounds (row);
        r.removeFromBottom (8);
    }

    // fader à esquerda, VUs à direita
    auto meters = r.removeFromRight (config.stereo ? 18 : 9).withTrimmedBottom (26);
    if (config.stereo)
    {
        meterL.setBounds (meters.removeFromLeft (8));
        meters.removeFromLeft (2);
        meterR.setBounds (meters.removeFromLeft (8));
    }
    else
    {
        meterL.setBounds (meters);
    }
    r.removeFromRight (6);
    fader.setBounds (r);
}

} // namespace bb
