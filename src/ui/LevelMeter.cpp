#include "ui/LevelMeter.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include "ui/Theme.h"

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

    if (clipFrames > 0)
        --clipFrames;

    repaint();
}

void LevelMeter::setClipped (bool clippedNow)
{
    if (clippedNow)
        clipFrames = 60; // ~2 s a 30 quadros/s
}

void LevelMeter::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float corner = juce::jmin (4.0f, (vertical ? r.getWidth() : r.getHeight()) * 0.5f);

    g.setColour (theme::field);
    g.fillRoundedRectangle (r, corner);

    const float p = toProportion (levelDb);
    const auto colour = levelDb > -1.0f ? theme::danger : levelDb > -6.0f ? theme::warn : barColour;
    g.setColour (colour);

    if (vertical)
        g.fillRoundedRectangle (r.withTop (r.getBottom() - r.getHeight() * p), corner);
    else
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * p), corner);

    if (holdDb > floorDb)
    {
        const float hp = toProportion (holdDb);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        if (vertical)
            g.fillRect (juce::Rectangle<float> (r.getX(), r.getBottom() - r.getHeight() * hp - 1.0f, r.getWidth(), 2.0f));
        else
            g.fillRect (juce::Rectangle<float> (r.getX() + r.getWidth() * hp - 1.0f, r.getY(), 2.0f, r.getHeight()));
    }

    g.setColour (clipFrames > 0 ? theme::danger : theme::border);
    g.drawRoundedRectangle (r, corner, 1.0f);
}

} // namespace bb
