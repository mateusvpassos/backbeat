#include "ui/WindowChrome.h"
#include "ui/Theme.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <dwmapi.h>
 #include <uxtheme.h>
 #pragma comment (lib, "dwmapi.lib")
 #pragma comment (lib, "uxtheme.lib")
#endif

namespace bb
{

void darkTitleBar (juce::Component& window, bool hideCaption)
{
   #if JUCE_WINDOWS
    auto* peer = window.getPeer();
    if (peer == nullptr)
        return;
    auto hwnd = (HWND) peer->getNativeHandle();

    const auto ref = [] (juce::Colour c) { return (COLORREF) RGB (c.getRed(), c.getGreen(), c.getBlue()); };
    const BOOL dark = TRUE;
    const auto bgRef = ref (theme::bg), borderRef = ref (theme::border), textRef = ref (theme::muted);
    DwmSetWindowAttribute (hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof (dark));
    DwmSetWindowAttribute (hwnd, 35 /* DWMWA_CAPTION_COLOR */, &bgRef, sizeof (bgRef));
    DwmSetWindowAttribute (hwnd, 34 /* DWMWA_BORDER_COLOR */, &borderRef, sizeof (borderRef));
    DwmSetWindowAttribute (hwnd, 36 /* DWMWA_TEXT_COLOR */, &textRef, sizeof (textRef));

    if (hideCaption)
    {
        WTA_OPTIONS options { WTNCA_NODRAWCAPTION | WTNCA_NODRAWICON, WTNCA_NODRAWCAPTION | WTNCA_NODRAWICON };
        SetWindowThemeAttribute (hwnd, WTA_NONCLIENT, &options, sizeof (options));
        // O Windows 11 ainda desenha o ícone pequeno: tira o ícone pequeno (barra de tarefas e
        // Alt+Tab usam o grande).
        SendMessage (hwnd, WM_SETICON, ICON_SMALL, 0);
        SetClassLongPtr (hwnd, GCLP_HICONSM, 0);
    }
   #else
    juce::ignoreUnused (window, hideCaption);
   #endif
}

} // namespace bb
