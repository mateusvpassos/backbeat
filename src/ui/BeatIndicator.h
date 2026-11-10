#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bb
{

// Bolinhas do compasso do metrônomo: acende o tempo atual (o tempo 1 na cor do Backbeat,
// a contagem em amarelo). Apagado quando o metrônomo não está tocando.
class BeatIndicator final : public juce::Component
{
public:
    void setState (int beatsPerBar, int activeBeat, bool countingIn, bool running);
    void paint (juce::Graphics&) override;

private:
    int numBeats = 4, active = -1;
    bool countIn = false, on = false;
};

} // namespace bb
