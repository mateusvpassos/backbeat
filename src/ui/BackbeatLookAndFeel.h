#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Theme.h"

namespace bb
{

// Visual da família (style.css do Riff/Rubato/Stinger...) traduzido para JUCE:
// fonte Geist, botões --surface com borda, interruptores no lugar de checkboxes, listas escuras,
// sliders com trilho fino e preenchimento em gradiente, faders verticais para o mixer.
class BackbeatLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    BackbeatLookAndFeel();

    static juce::Typeface::Ptr typeface (theme::Weight weight);
    static juce::Typeface::Ptr monoTypeface();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    // Botões (TextButton). Propriedade "variant": "primary" (gradiente) ou "ghost".
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    // Interruptor (.switch)
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    // Listas
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcut, const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    juce::Font getPopupMenuFont() override;

    // Sliders e faders
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    // Campos de texto
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;

    // Abas (janela de mapeamento) no estilo .seg
    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int w, int h) override;
    int getTabButtonBestWidth (juce::TabBarButton&, int tabDepth) override;

    int getDefaultScrollbarWidth() override { return 10; }
};

} // namespace bb
