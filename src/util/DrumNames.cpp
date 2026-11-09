#include "util/DrumNames.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace bb
{

juce::String gmDrumName (int n)
{
    switch (n)
    {
        case 35: return "Bumbo 2";
        case 36: return "Bumbo";
        case 37: return "Aro (cross stick)";
        case 38: return "Caixa";
        case 39: return "Palmas";
        case 40: return "Caixa (aro)";
        case 41: return "Surdo grave";
        case 42: return "Chimbal fechado";
        case 43: return "Surdo";
        case 44: return L"Chimbal no pé";
        case 45: return "Tom grave";
        case 46: return "Chimbal aberto";
        case 47: return L"Tom médio-grave";
        case 48: return L"Tom médio-agudo";
        case 49: return "Prato de ataque 1";
        case 50: return "Tom agudo";
        case 51: return L"Prato de condução 1";
        case 52: return "Prato china";
        case 53: return L"Cúpula da condução";
        case 54: return "Pandeirola";
        case 55: return "Splash";
        case 56: return "Cowbell";
        case 57: return "Prato de ataque 2";
        case 59: return L"Prato de condução 2";
        default: return {};
    }
}

juce::String describeDrumNote (int n)
{
    auto text = juce::String (n) + L" · " + juce::MidiMessage::getMidiNoteName (n, true, true, 3);
    const auto name = gmDrumName (n);
    if (name.isNotEmpty())
        text << L" · " << name;
    return text;
}

} // namespace bb
