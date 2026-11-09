#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bb::theme
{

inline const juce::Colour background  { 0xff121418 };
inline const juce::Colour panel       { 0xff1a1d23 };
inline const juce::Colour panelLight  { 0xff232731 };
inline const juce::Colour accent      { 0xfff0673a }; // laranja "baqueta"
inline const juce::Colour textDim     { 0xff8b93a1 };
inline const juce::Colour text        { 0xffe6e8ec };
inline const juce::Colour error       { 0xffe5484d };
inline const juce::Colour ok          { 0xff3fb67b };

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

inline void header (juce::Label& l, const juce::String& t)
{
    l.setText (upper (t), juce::dontSendNotification);
    l.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    l.setColour (juce::Label::textColourId, textDim);
}

inline void caption (juce::Label& l, const juce::String& t)
{
    l.setText (t, juce::dontSendNotification);
    l.setFont (juce::Font (juce::FontOptions (13.0f)));
    l.setColour (juce::Label::textColourId, textDim);
}

inline void body (juce::Label& l, const juce::String& t = {})
{
    l.setText (t, juce::dontSendNotification);
    l.setFont (juce::Font (juce::FontOptions (15.0f)));
    l.setColour (juce::Label::textColourId, text);
}

} // namespace bb::theme
