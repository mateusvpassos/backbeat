#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bb
{

// VU horizontal simples em dB (−60 a 0), com retenção de pico. A UI alimenta com setPeak() ~30x/s.
class LevelMeter final : public juce::Component
{
public:
    void setPeak (float linearPeak);
    void paint (juce::Graphics&) override;

private:
    float levelDb = -100.0f;
    float holdDb = -100.0f;
    int holdFrames = 0;
};

} // namespace bb
