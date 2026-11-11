#include "ui/BackbeatLookAndFeel.h"

#include "BackbeatAssets.h"
#include "ui/Icons.h"

namespace bb
{

// ---------------------------------------------------------------------------
// Fontes (Geist, OFL — os mesmos arquivos dos outros apps, em TTF estático)
// ---------------------------------------------------------------------------

namespace
{
    juce::Typeface::Ptr load (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    juce::Colour onColourOf (juce::Button& b, juce::Colour fallback)
    {
        const auto v = b.getProperties()["onColour"];
        return v.isVoid() ? fallback : juce::Colour ((juce::uint32) (juce::int64) v);
    }
}

juce::Typeface::Ptr BackbeatLookAndFeel::typeface (theme::Weight weight)
{
    static auto regular  = load (BackbeatAssets::GeistRegular_ttf, BackbeatAssets::GeistRegular_ttfSize);
    static auto medium   = load (BackbeatAssets::GeistMedium_ttf, BackbeatAssets::GeistMedium_ttfSize);
    static auto semibold = load (BackbeatAssets::GeistSemiBold_ttf, BackbeatAssets::GeistSemiBold_ttfSize);

    switch (weight)
    {
        case theme::Weight::medium:   return medium;
        case theme::Weight::semibold: return semibold;
        case theme::Weight::regular:  break;
    }
    return regular;
}

juce::Typeface::Ptr BackbeatLookAndFeel::monoTypeface()
{
    static auto mono = load (BackbeatAssets::GeistMonoMedium_ttf, BackbeatAssets::GeistMonoMedium_ttfSize);
    return mono;
}

juce::Font theme::font (float height, Weight weight)
{
    return juce::Font (juce::FontOptions (BackbeatLookAndFeel::typeface (weight)).withHeight (height));
}

juce::Font theme::mono (float height)
{
    return juce::Font (juce::FontOptions (BackbeatLookAndFeel::monoTypeface()).withHeight (height));
}

juce::Typeface::Ptr BackbeatLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    const auto name = f.getTypefaceName();
    const auto style = f.getTypefaceStyle();

    if (name == juce::Font::getDefaultMonospacedFontName() || name.startsWith ("Geist Mono"))
        return monoTypeface();

    if (name == juce::Font::getDefaultSansSerifFontName() || name == juce::Font::getDefaultSerifFontName()
        || name.startsWith ("Geist"))
    {
        if (f.isBold() || style.containsIgnoreCase ("Bold") || style.containsIgnoreCase ("Semi"))
            return typeface (theme::Weight::semibold);
        if (style.containsIgnoreCase ("Medium"))
            return typeface (theme::Weight::medium);
        return typeface (theme::Weight::regular);
    }

    return LookAndFeel_V4::getTypefaceForFont (f);
}

// ---------------------------------------------------------------------------
// Cores
// ---------------------------------------------------------------------------

BackbeatLookAndFeel::BackbeatLookAndFeel()
{
    using namespace theme;

    setColourScheme ({ bg, panel, surface, border2, text, accent, text, surfaceHover, text });

    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::DocumentWindow::backgroundColourId, bg);

    setColour (juce::TextButton::buttonColourId, surface);
    setColour (juce::TextButton::buttonOnColourId, accentSoft());
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, accentText);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, accent);

    setColour (juce::ComboBox::backgroundColourId, field);
    setColour (juce::ComboBox::outlineColourId, border2);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, muted);
    setColour (juce::ComboBox::focusedOutlineColourId, accent);

    setColour (juce::PopupMenu::backgroundColourId, menu);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, muted);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, surfaceHover);
    setColour (juce::PopupMenu::highlightedTextColourId, text);

    setColour (juce::Label::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, field);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, border2);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::TextEditor::highlightColourId, accentSoft());
    setColour (juce::CaretComponent::caretColourId, accent);

    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff2c2f3d));

    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, field);
    setColour (juce::Slider::textBoxOutlineColourId, border);
    setColour (juce::Slider::textBoxHighlightColourId, accentSoft());
    setColour (juce::Slider::thumbColourId, text);
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::backgroundColourId, field);

    setColour (juce::TooltipWindow::backgroundColourId, menu);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, border2);

    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, border2);

    setColour (juce::TabbedComponent::backgroundColourId, bg);
    setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TabbedButtonBar::frontOutlineColourId, juce::Colours::transparentBlack);
}

// ---------------------------------------------------------------------------
// Botões
// ---------------------------------------------------------------------------

void BackbeatLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                                bool highlighted, bool down)
{
    using namespace theme;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    if (down)
        r = r.translated (0.0f, 1.0f);

    const auto variant = b.getProperties()["variant"].toString();
    const bool on = b.getToggleState();
    const float alpha = b.isEnabled() ? 1.0f : 0.35f;

    // .primary: --grad (só um botão principal por tela, guia ui-style).
    if (variant == "primary" && ! on)
    {
        juce::ColourGradient grad (gradTop, r.getX(), r.getY(), gradBottom, r.getRight(), r.getBottom(), false);
        grad.addColour (0.52, accent);
        g.setGradientFill (grad);
        g.setOpacity (alpha * (highlighted ? 1.0f : 0.94f));
        g.fillRoundedRectangle (r, radius);
        return;
    }

    juce::Colour fill = surface, line = border2;

    if (on)
    {
        const auto c = onColourOf (b, accent);
        fill = c.withAlpha (highlighted ? 0.24f : 0.16f);
        line = c.withAlpha (0.55f);
    }
    else if (variant == "ghost")
    {
        fill = highlighted ? surface : juce::Colours::transparentBlack;
        line = juce::Colours::transparentBlack;
    }
    else if (highlighted)
    {
        fill = surfaceHover;
        line = juce::Colour (0xff3e4358);
    }

    g.setColour (fill.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (r, radius);
    g.setColour (line.withMultipliedAlpha (alpha));
    g.drawRoundedRectangle (r, radius, 1.0f);
}

// button: 12.5px, peso 500 (primary: 600).
juce::Font BackbeatLookAndFeel::getTextButtonFont (juce::TextButton& b, int buttonHeight)
{
    const auto weight = b.getProperties()["variant"].toString() == "primary" ? theme::Weight::semibold : theme::Weight::medium;
    return theme::font (juce::jmin (12.5f, buttonHeight * 0.46f), weight);
}

void BackbeatLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    using namespace theme;
    const auto variant = b.getProperties()["variant"].toString();
    const bool on = b.getToggleState();

    juce::Colour c = text;
    if (on)
        c = b.getProperties().contains ("onColour") ? onColourOf (b, accent).brighter (0.35f) : accentText;
    else if (variant == "primary")
        c = juce::Colour (0xff2b1404);
    else if (variant == "ghost")
        c = muted;

    c = c.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.35f);
    const auto font = getTextButtonFont (b, b.getHeight());
    auto r = b.getLocalBounds().reduced (6, 0);
    if (down)
        r = r.translated (0, 1);

    // Ícone opcional (propriedade "icon"): 15px à esquerda do texto, o par centrado, gap 6px.
    const auto iconName = b.getProperties()["icon"].toString();
    if (iconName.isNotEmpty())
    {
        constexpr float iconSize = 15.0f, gap = 6.0f;
        const auto label = b.getButtonText();
        const float textW = label.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (font, label);
        const float total = juce::jmin ((float) r.getWidth(), iconSize + (label.isEmpty() ? 0.0f : gap + textW));
        auto row = r.toFloat().withSizeKeepingCentre (total, (float) r.getHeight());
        const auto iconColour = (iconName == "record" && ! on) ? danger.withMultipliedAlpha (c.getFloatAlpha()) : c;
        icons::draw (g, iconName, row.removeFromLeft (iconSize).withSizeKeepingCentre (iconSize, iconSize), iconColour);
        if (label.isNotEmpty())
        {
            row.removeFromLeft (gap);
            g.setColour (c);
            g.setFont (font);
            g.drawFittedText (label, row.toNearestInt(), juce::Justification::centredLeft, 1, 0.9f);
        }
        return;
    }

    g.setColour (c);
    g.setFont (font);
    g.drawFittedText (b.getButtonText(), r, juce::Justification::centred, 1, 0.9f);
}

// .switch: trilho de 30x17 com bolinha de 13px.
void BackbeatLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    using namespace theme;
    const bool on = b.getToggleState();
    const float alpha = b.isEnabled() ? 1.0f : 0.35f;
    const auto h = (float) b.getHeight();

    const juce::Rectangle<float> track (1.0f, (h - 17.0f) * 0.5f, 30.0f, 17.0f);
    g.setColour ((on ? accent : juce::Colour (0xff2c2f3e)).withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (track, 8.5f);
    if (highlighted && ! on)
    {
        g.setColour (border2.withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (track, 8.5f, 1.0f);
    }

    const float knobX = on ? track.getX() + 15.0f : track.getX() + 2.0f;
    g.setColour ((on ? juce::Colours::white : juce::Colour (0xffcfd2dc)).withMultipliedAlpha (alpha));
    g.fillEllipse (knobX, track.getY() + 2.0f, 13.0f, 13.0f);

    g.setColour (text.withMultipliedAlpha (alpha));
    g.setFont (font (13.0f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (40), juce::Justification::centredLeft, 1);
}

// ---------------------------------------------------------------------------
// Listas
// ---------------------------------------------------------------------------

void BackbeatLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace theme;
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    const float alpha = box.isEnabled() ? 1.0f : 0.35f;

    g.setColour (field.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (r, radius);
    g.setColour ((box.hasKeyboardFocus (true) || box.isPopupActive() ? accent : border2).withMultipliedAlpha (alpha));
    g.drawRoundedRectangle (r, radius, 1.0f);

    // chevron "m6 9 6 6 6-6"
    const float cx = (float) w - 16.0f, cy = (float) h * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.5f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.5f);
    chevron.lineTo (cx + 4.5f, cy - 2.0f);
    g.setColour (muted.withMultipliedAlpha (alpha));
    g.strokePath (chevron, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font BackbeatLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return theme::font (13.0f);
}

void BackbeatLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 32, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void BackbeatLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (theme::menu);
    g.setColour (theme::border2);
    g.drawRect (0, 0, w, h, 1);
}

void BackbeatLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                             bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                             const juce::String& itemText, const juce::String& shortcut,
                                             const juce::Drawable* icon, const juce::Colour* textColour)
{
    using namespace theme;

    if (isSeparator)
    {
        g.setColour (border2);
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }

    const auto r = area.reduced (4, 1).toFloat();
    if (isTicked)
    {
        g.setColour (accentSoft());
        g.fillRoundedRectangle (r, 6.0f);
    }
    if (isHighlighted && isActive)
    {
        g.setColour (isTicked ? accentSoft().withMultipliedAlpha (1.6f) : surfaceHover);
        g.fillRoundedRectangle (r, 6.0f);
    }

    juce::Colour c = isTicked ? accentText : (textColour != nullptr ? *textColour : text);
    if (! isActive)
        c = c.withMultipliedAlpha (0.35f);
    g.setColour (c);
    g.setFont (getPopupMenuFont());

    auto textArea = area.reduced (12, 0);
    if (icon != nullptr)
        textArea.removeFromLeft (22);
    g.drawFittedText (itemText, textArea, juce::Justification::centredLeft, 1);

    if (shortcut.isNotEmpty())
    {
        g.setColour (muted);
        g.setFont (mono (11.0f));
        g.drawText (shortcut, textArea, juce::Justification::centredRight);
    }

    if (hasSubMenu)
    {
        g.setColour (muted);
        g.drawText (juce::String::fromUTF8 ("\xe2\x80\xba"), textArea, juce::Justification::centredRight); // ›
    }
}

void BackbeatLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area,
                                                      const juce::String& sectionName)
{
    g.setColour (theme::muted);
    g.setFont (theme::font (11.0f, theme::Weight::semibold).withExtraKerningFactor (0.06f));
    g.drawFittedText (theme::upper (sectionName), area.reduced (12, 0).withTrimmedTop (4),
                      juce::Justification::bottomLeft, 1);
}

juce::Font BackbeatLookAndFeel::getPopupMenuFont()
{
    return theme::font (13.0f);
}

// ---------------------------------------------------------------------------
// Sliders e faders
// ---------------------------------------------------------------------------

void BackbeatLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos,
                                            float maxPos, juce::Slider::SliderStyle style, juce::Slider& s)
{
    using namespace theme;
    const float alpha = s.isEnabled() ? 1.0f : 0.35f;

    if (style == juce::Slider::LinearHorizontal)
    {
        const float cy = (float) y + (float) h * 0.5f;
        const juce::Rectangle<float> track ((float) x, cy - 2.5f, (float) w, 5.0f);
        g.setColour (field.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (track, 2.5f);
        g.setColour (border.withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (track, 2.5f, 1.0f);

        const auto filled = track.withRight (pos);
        juce::ColourGradient grad (gradTop, track.getX(), cy, gradBottom, track.getRight(), cy, false);
        grad.addColour (0.52, accent);
        g.setGradientFill (grad);
        g.setOpacity (alpha);
        g.fillRoundedRectangle (filled, 2.5f);
        g.setOpacity (1.0f);

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillEllipse (pos - 7.0f, cy - 6.0f, 14.0f, 14.0f);
        g.setColour (text.withMultipliedAlpha (alpha));
        g.fillEllipse (pos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
        return;
    }

    if (style == juce::Slider::LinearVertical)
    {
        // Fader do mixer: trilho fino com o preenchimento na cor da faixa e capa retangular.
        const auto trackColour = s.findColour (juce::Slider::trackColourId);
        const float cx = (float) x + (float) w * 0.5f;
        const juce::Rectangle<float> track (cx - 2.5f, (float) y, 5.0f, (float) h);
        g.setColour (field.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (track, 2.5f);
        g.setColour (border.withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (track, 2.5f, 1.0f);

        g.setColour (trackColour.withMultipliedAlpha (alpha * 0.85f));
        g.fillRoundedRectangle (track.withTop (pos), 2.5f);

        // marca do 0 dB, se o slider disser onde fica
        if (s.getProperties().contains ("unityPos"))
        {
            const float unity = (float) s.getPositionOfValue ((double) s.getProperties()["unityPos"]);
            g.setColour (dim);
            g.fillRect (cx - 9.0f, unity, 18.0f, 1.0f);
        }

        const juce::Rectangle<float> cap (cx - 13.0f, pos - 7.0f, 26.0f, 14.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (cap.translated (0.0f, 1.5f), 4.0f);
        g.setColour (surfaceHover.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (cap, 4.0f);
        g.setColour (border2.withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (cap, 4.0f, 1.0f);
        g.setColour (text.withMultipliedAlpha (alpha * 0.8f));
        g.fillRect (cap.getX() + 6.0f, pos - 0.5f, cap.getWidth() - 12.0f, 1.0f);
        return;
    }

    LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
}

juce::Label* BackbeatLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (theme::mono (11.5f));
    l->setColour (juce::Label::outlineColourId, theme::border);
    l->setColour (juce::Label::backgroundColourId, theme::field);
    l->setJustificationType (juce::Justification::centred);
    return l;
}

// ---------------------------------------------------------------------------
// Campos de texto e abas
// ---------------------------------------------------------------------------

void BackbeatLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f), theme::radius);
}

void BackbeatLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? theme::accent : theme::border2);
    g.drawRoundedRectangle (juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f), theme::radius, 1.0f);
}

void BackbeatLookAndFeel::drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool isMouseOver, bool)
{
    using namespace theme;
    const bool front = b.isFrontTab();
    auto r = b.getActiveArea().toFloat().reduced (3.0f, 5.0f);

    if (front)
    {
        g.setColour (surfaceHover);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (border2);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }
    else if (isMouseOver)
    {
        g.setColour (surface);
        g.fillRoundedRectangle (r, 6.0f);
    }

    g.setColour (front ? text : muted);
    g.setFont (font (13.0f, Weight::medium));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void BackbeatLookAndFeel::drawTabAreaBehindFrontButton (juce::TabbedButtonBar& bar, juce::Graphics& g, int w, int h)
{
    g.setColour (theme::border);
    g.fillRect (0, h - 1, w, 1);
    juce::ignoreUnused (bar);
}

int BackbeatLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& b, int)
{
    return juce::GlyphArrangement::getStringWidthInt (theme::font (13.0f, theme::Weight::medium), b.getButtonText()) + 36;
}

} // namespace bb
