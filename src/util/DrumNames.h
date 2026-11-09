#pragma once

#include <juce_core/juce_core.h>

namespace bb
{

// Nome da peça no padrão General MIDI (canal 10), em português. Vazio se não for uma nota de bateria GM.
juce::String gmDrumName (int noteNumber);

// "38 · D1 · Caixa" — usado nos displays de nota.
juce::String describeDrumNote (int noteNumber);

} // namespace bb
