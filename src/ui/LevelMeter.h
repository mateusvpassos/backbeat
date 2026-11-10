#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bb
{

// VU no estilo .st-meter da família: barra na cor da faixa, amarela acima de -6 dB, vermelha perto de 0,
// marca de pico e borda vermelha quando houve clip (fica 2 s). A UI alimenta com setPeak() ~30x/s.
class LevelMeter final : public juce::Component
{
public:
    void setVertical (bool v)            { vertical = v; repaint(); }
    void setBarColour (juce::Colour c)   { barColour = c; repaint(); }
    void setPeak (float linearPeak);
    void setClipped (bool clippedNow);   // pico de 0 dBFS desde a última leitura

    void paint (juce::Graphics&) override;

private:
    bool vertical = false;
    juce::Colour barColour { 0xffff9548 };
    float levelDb = -100.0f;
    float holdDb = -100.0f;
    int holdFrames = 0;
    int clipFrames = 0;
};

} // namespace bb
