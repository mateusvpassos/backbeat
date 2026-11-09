#include "ui/LevelMeter.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace bb
{

namespace
{
    constexpr float floorDb = -60.0f;
    float toProportion (float db) { return juce::jlimit (0.0f, 1.0f, (db - floorDb) / -floorDb); }
}

void LevelMeter::setPeak (float linearPeak)
{
    const float db = juce::Decibels::gainToDecibels (linearPeak, -100.0f);

    // Sobe na hora, desce ~1,5 dB por quadro.
    levelDb = db > levelDb ? db : juce::jmax (db, levelDb - 1.5f);

    if (db >= holdDb)
    {
        holdDb = db;
        holdFrames = 45;
    }
    else if (--holdFrames <= 0)
    {
        holdDb = juce::jmax (levelDb, holdDb - 1.0f);
    }

    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff2c313c));
    g.fillRoundedRectangle (r, 3.0f);

    const float w = r.getWidth() * toProportion (levelDb);
    const auto barColour = levelDb > -1.0f ? juce::Colour (0xffe5484d)
                         : levelDb > -12.0f ? juce::Colour (0xfff5a524)
                                            : juce::Colour (0xff3fb67b);
    g.setColour (barColour);
    g.fillRoundedRectangle (r.withWidth (w), 3.0f);

    if (holdDb > floorDb)
    {
        const float x = r.getX() + r.getWidth() * toProportion (holdDb);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillRect (juce::Rectangle<float> (x - 1.0f, r.getY(), 2.0f, r.getHeight()));
    }
}

} // namespace bb
