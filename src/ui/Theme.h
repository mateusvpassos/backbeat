#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Tema da família Kino/Strata/Lembreme/Overtone/Stinger/Riff/Rubato (mesmas variáveis do style.css),
// com a cor própria do Backbeat: laranja-âmbar (baqueta).
namespace bb::theme
{

// ---- base comum da família ----
inline const juce::Colour bg           { 0xff0d0e13 };
inline const juce::Colour panel        { 0xff14151c };
inline const juce::Colour panel2       { 0xff1b1d26 };
inline const juce::Colour surface      { 0xff20222d };
inline const juce::Colour surfaceHover { 0xff282b38 };
inline const juce::Colour border       { 0xff252836 };
inline const juce::Colour border2      { 0xff323648 };
inline const juce::Colour field        { 0xff0f1016 };
inline const juce::Colour menu         { 0xff1d1f29 };
inline const juce::Colour text         { 0xffe9eaf1 };
inline const juce::Colour muted        { 0xff8b90a4 };
inline const juce::Colour dim          { 0xff5c6176 };
inline const juce::Colour danger       { 0xffff5d6c };
inline const juce::Colour ok           { 0xff3ddc97 };
inline const juce::Colour warn         { 0xffffbf47 };

// ---- cor do Backbeat ----
inline const juce::Colour accent       { 0xffff9548 };
inline const juce::Colour accentText   { 0xffffd9b0 };
inline const juce::Colour gradTop      { 0xffffd9a3 };
inline const juce::Colour gradBottom   { 0xffec5f1f };
inline juce::Colour accentSoft()       { return accent.withAlpha (0.14f); }
inline juce::Colour accentLine()       { return accent.withAlpha (0.5f); }

// Cores das faixas do mixer (cada uma lembra um irmão da família).
inline const juce::Colour trackDrums   { 0xffff9548 };
inline const juce::Colour trackChrome  { 0xff22c3e6 };
inline const juce::Colour trackInput1  { 0xff2dd4bf };
inline const juce::Colour trackInput2  { 0xff8b7bff };

constexpr float radius = 8.0f;       // --radius
constexpr float panelRadius = 12.0f; // .panel

// ---- nomes antigos (telas de mapeamento) ----
inline const juce::Colour& background = bg;
inline const juce::Colour& panelLight = panel2;
inline const juce::Colour& textDim = muted;
inline const juce::Colour& error = danger;

// ---- fontes (Geist, a mesma dos irmãos; carregada em BackbeatLookAndFeel) ----
enum class Weight { regular, medium, semibold };
juce::Font font (float height, Weight weight = Weight::regular);
juce::Font mono (float height);

// Maiúsculas com os acentos do português (o toUpperCase do JUCE deixa "ç", "í" etc. minúsculos).
inline juce::String upper (const juce::String& t)
{
    auto s = t.toUpperCase();
    const wchar_t* lower = L"áàâãéêíóôõúüç";
    const wchar_t* upperChars = L"ÁÀÂÃÉÊÍÓÔÕÚÜÇ";
    for (int i = 0; lower[i] != 0; ++i)
        s = s.replaceCharacter ((juce::juce_wchar) lower[i], (juce::juce_wchar) upperChars[i]);
    return s;
}

// .lbl: 11px, 600, maiúsculas espaçadas, cinza.
inline void header (juce::Label& l, const juce::String& t)
{
    l.setText (upper (t), juce::dontSendNotification);
    l.setFont (font (11.5f, Weight::semibold).withExtraKerningFactor (0.06f));
    l.setColour (juce::Label::textColourId, muted);
}

inline void caption (juce::Label& l, const juce::String& t)
{
    l.setText (t, juce::dontSendNotification);
    l.setFont (font (12.5f));
    l.setColour (juce::Label::textColourId, muted);
}

inline void body (juce::Label& l, const juce::String& t = {})
{
    l.setText (t, juce::dontSendNotification);
    l.setFont (font (13.5f));
    l.setColour (juce::Label::textColourId, text);
}

// Painel da família: fundo --panel, borda 1px --border, cantos de 12px.
inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (panel);
    g.fillRoundedRectangle (r, panelRadius);
    g.setColour (border);
    g.drawRoundedRectangle (r.reduced (0.5f), panelRadius, 1.0f);
}

} // namespace bb::theme
