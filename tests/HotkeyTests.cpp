#include "input/GlobalHotkeys.h"

namespace bb
{

// Regressão: o F9 (0x78) chegou a ser registrado como 'X' (0x58), e F8/F10 como 'W'/'Y',
// prendendo essas letras no Windows inteiro enquanto o Backbeat estava aberto.
class HotkeyTest final : public juce::UnitTest
{
public:
    HotkeyTest() : juce::UnitTest ("Atalhos globais", "input") {}

    void runTest() override
    {
        beginTest (L"teclas F e letras viram o código virtual certo");

        const auto vkOf = [] (const juce::String& description, unsigned& mods)
        {
            unsigned vk = 0;
            mods = 0;
            GlobalHotkeys::toVirtualKey (juce::KeyPress::createFromDescription (description), vk, mods);
            return (int) vk;
        };

        unsigned mods = 0;
        expectEquals (vkOf ("F8", mods), 0x77);   // VK_F8
        expectEquals (vkOf ("F9", mods), 0x78);   // VK_F9 (não 'X')
        expectEquals (vkOf ("F10", mods), 0x79);  // VK_F10
        expectEquals (vkOf ("F12", mods), 0x7B);
        expectEquals (vkOf ("Z", mods), 0x5A);
        expectEquals (vkOf ("z", mods), 0x5A);

        expectEquals (vkOf ("ctrl + R", mods), 0x52);
        expect ((mods & 0x0002) != 0, "Ctrl precisa virar MOD_CONTROL");
        expectEquals (vkOf ("alt + shift + F9", mods), 0x78);
        expect ((mods & 0x0001) != 0 && (mods & 0x0004) != 0, "Alt e Shift precisam ir junto");
    }
};

static HotkeyTest hotkeyTest;

} // namespace bb
