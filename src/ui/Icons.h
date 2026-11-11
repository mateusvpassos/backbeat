#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Ícones da família (Lucide, 24x24, traço 2). Nomes: gear, plug, layers, save, zap, info, record,
// play, stop, speaker, drum, metronome, waves, sliders, folder, download.
namespace bb::icons
{
const juce::Drawable* get (const juce::String& name, juce::Colour colour);
void draw (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area, juce::Colour colour);
}
