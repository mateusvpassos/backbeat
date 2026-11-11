// Ícones de traço 24x24 (Lucide, ISC), os mesmos dos outros apps da família (icons.js).
#include "ui/Icons.h"

#include <map>

namespace bb::icons
{
namespace
{
    struct Entry { const char* name; const char* body; };

    const Entry entries[] = {
        { "gear", R"svg(<circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/>)svg" },
        { "plug", R"svg(<path d="M12 22v-5"/><path d="M9 8V2"/><path d="M15 8V2"/><path d="M18 8v5a4 4 0 0 1-4 4h-4a4 4 0 0 1-4-4V8Z"/>)svg" },
        { "layers", R"svg(<polygon points="12 2 2 7 12 12 22 7 12 2"/><polyline points="2 17 12 22 22 17"/><polyline points="2 12 12 17 22 12"/>)svg" },
        { "save", R"svg(<path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/><polyline points="7 3 7 8 15 8"/>)svg" },
        { "zap", R"svg(<path d="M13 2 3 14h9l-1 8 10-12h-9z"/>)svg" },
        { "info", R"svg(<circle cx="12" cy="12" r="10"/><path d="M12 16v-4"/><path d="M12 8h.01"/>)svg" },
        { "record", R"svg(<circle cx="12" cy="12" r="7" fill="currentColor" stroke="none"/>)svg" },
        { "play", R"svg(<polygon points="6 3 20 12 6 21 6 3"/>)svg" },
        { "stop", R"svg(<rect x="5" y="5" width="14" height="14" rx="2"/>)svg" },
        { "speaker", R"svg(<rect x="4" y="2" width="16" height="20" rx="2"/><circle cx="12" cy="14" r="4"/><line x1="12" y1="6" x2="12.01" y2="6"/>)svg" },
        { "drum", R"svg(<path d="m2 2 8 8"/><path d="m22 2-8 8"/><ellipse cx="12" cy="9" rx="10" ry="5"/><path d="M7 13.4v7.9"/><path d="M12 14v8"/><path d="M17 13.4v7.9"/><path d="M2 9v8a10 5 0 0 0 20 0V9"/>)svg" },
        { "metronome", R"svg(<path d="M12 11.4V9.1"/><path d="m12 17 6.59-6.59"/><path d="m15.05 5.7-.218-.691a3 3 0 0 0-5.663 0L4.418 19.695A1 1 0 0 0 5.37 21h13.253a1 1 0 0 0 .951-1.31L18.45 16.2"/><circle cx="20" cy="9" r="2"/>)svg" },
        { "waves", R"svg(<path d="M2 6c.6.5 1.2 1 2.5 1C7 7 7 5 9.5 5c2.6 0 2.4 2 5 2 2.5 0 2.5-2 5-2 1.3 0 1.9.5 2.5 1"/><path d="M2 12c.6.5 1.2 1 2.5 1 2.5 0 2.5-2 5-2 2.6 0 2.4 2 5 2 2.5 0 2.5-2 5-2 1.3 0 1.9.5 2.5 1"/><path d="M2 18c.6.5 1.2 1 2.5 1 2.5 0 2.5-2 5-2 2.6 0 2.4 2 5 2 2.5 0 2.5-2 5-2 1.3 0 1.9.5 2.5 1"/>)svg" },
        { "sliders", R"svg(<line x1="4" y1="21" x2="4" y2="14"/><line x1="4" y1="10" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="12"/><line x1="12" y1="8" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="16"/><line x1="20" y1="12" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="9" y1="8" x2="15" y2="8"/><line x1="17" y1="16" x2="23" y2="16"/>)svg" },
        { "folder", R"svg(<path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"/>)svg" },
        { "download", R"svg(<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/>)svg" },
    };

    const char* bodyOf (const juce::String& name)
    {
        for (const auto& e : entries)
            if (name == e.name)
                return e.body;
        return nullptr;
    }
}

const juce::Drawable* get (const juce::String& name, juce::Colour colour)
{
    // Cache por nome + cor (paint chama muitas vezes; criar do SVG a cada vez seria lento).
    static std::map<juce::String, std::unique_ptr<juce::Drawable>> cache;
    const auto key = name + "#" + colour.toString();
    if (auto it = cache.find (key); it != cache.end())
        return it->second.get();

    const auto* body = bodyOf (name);
    if (body == nullptr)
        return nullptr;

    const auto hex = "#" + colour.toDisplayString (false);
    const auto svg = juce::String ("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"")
                   + hex + "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
                   + juce::String (body).replace ("currentColor", hex) + "</svg>";

    std::unique_ptr<juce::Drawable> d;
    if (auto xml = juce::parseXML (svg))
        d = juce::Drawable::createFromSVG (*xml);
    return (cache[key] = std::move (d)).get();
}

void draw (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area, juce::Colour colour)
{
    // Escala pelo viewBox 24x24 (não pelo contorno do desenho), num quadrado centrado na área.
    if (auto* d = get (name, colour.withAlpha (1.0f)))
    {
        const auto side = juce::jmin (area.getWidth(), area.getHeight());
        const auto sq = area.withSizeKeepingCentre (side, side);
        d->draw (g, colour.getFloatAlpha(),
                 juce::AffineTransform::scale (side / 24.0f).translated (sq.getX(), sq.getY()));
    }
}

} // namespace bb::icons
