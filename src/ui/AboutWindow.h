#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bb
{

// Janela "Sobre": versão, atalhos do perfil ativo e licenças de terceiros.
// `shortcuts`: linhas "tecla|ação". `dataFolder`: pasta das configurações e do log.
void showAboutWindow (const juce::StringArray& shortcuts, const juce::File& dataFolder);

} // namespace bb
