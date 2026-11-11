#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bb
{
// Barra de título nativa na cor do fundo, com botões escuros (como o titleBarOverlay dos apps
// Electron da família). Chamar com a janela já visível. hideCaption: sem texto e ícone na barra
// (janela principal, que já mostra a marca no topo). Windows 11; no 10 fica só o modo escuro.
void darkTitleBar (juce::Component& window, bool hideCaption = false);
}
