#include "ui/BeatIndicator.h"

#include "ui/Theme.h"

namespace bb
{

void BeatIndicator::setState (int beatsPerBar, int activeBeat, bool countingIn, bool running)
{
    beatsPerBar = juce::jlimit (1, 16, beatsPerBar);
    if (! running)
        activeBeat = -1;

    if (beatsPerBar == numBeats && activeBeat == active && countingIn == countIn && running == on)
        return;

    numBeats = beatsPerBar;
    active = activeBeat;
    countIn = countingIn;
    on = running;
    repaint();
}

void BeatIndicator::paint (juce::Graphics& g)
{
    constexpr float d = 10.0f, gap = 7.0f;
    const float total = numBeats * d + (numBeats - 1) * gap;
    float x = (float) getWidth() - total;
    const float y = ((float) getHeight() - d) * 0.5f;

    for (int i = 0; i < numBeats; ++i, x += d + gap)
    {
        const juce::Rectangle<float> r (x, y, d, d);

        if (i == active)
        {
            const auto c = countIn ? theme::warn : (i == 0 ? theme::accent : theme::text);
            g.setColour (c.withAlpha (0.25f));
            g.fillEllipse (r.expanded (3.0f));
            g.setColour (c);
            g.fillEllipse (r);
        }
        else
        {
            g.setColour (i == 0 ? theme::border2.brighter (0.15f) : theme::border2);
            g.fillEllipse (r);
        }
    }
}

} // namespace bb
