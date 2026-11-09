#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/AudioEngine.h"
#include "mapping/DrumCatalog.h"
#include "mapping/ProfileManager.h"
#include "ui/Theme.h"

#include <functional>

namespace bb
{

// O que as telas de mapeamento precisam do app.
struct MappingContext
{
    ProfileManager& profiles;
    DrumCatalog& catalog;
    AudioEngine& engine;
    std::function<void()> chooseKitFile; // abre o seletor de .ssd5kit
};

namespace pieceCombo
{
    constexpr int ignoreId = 1000;
    constexpr int commandIdBase = 2000;

    inline int idForNote (int note) { return note >= 0 ? note + 1 : ignoreId; }
    inline int noteForId (int id)   { return (id >= 1 && id <= 128) ? id - 1 : -1; }

    // Preenche com as peças do catálogo agrupadas (Bumbo, Caixa, Tons...).
    inline void fill (juce::ComboBox& box, const DrumCatalog& catalog, bool withIgnore, bool withCommands)
    {
        box.clear (juce::dontSendNotification);

        if (withCommands)
        {
            box.addSectionHeading ("Comandos");
            for (auto c : allCommands())
                box.addItem (commandLabel (c), commandIdBase + (int) c);
        }

        for (const auto& group : catalog.groupsInOrder())
        {
            box.addSectionHeading (group);
            for (const auto& p : catalog.getPieces())
                if (p.group == group)
                    box.addItem (p.name + "  (" + juce::String (p.note) + ")", idForNote (p.note));
        }

        if (withIgnore)
        {
            box.addSeparator();
            box.addItem (L"Ignorar (silêncio)", ignoreId);
        }
    }

    // Seleciona a nota; se ela não está no catálogo, mostra "nota N".
    inline void select (juce::ComboBox& box, int note)
    {
        const int id = idForNote (note);
        if (box.indexOfItemId (id) < 0)
            box.setText ("nota " + juce::String (note), juce::dontSendNotification);
        else
            box.setSelectedId (id, juce::dontSendNotification);
    }
}

// Pergunta um texto (nome de perfil etc.) numa janela modal assíncrona.
inline void askText (const juce::String& title, const juce::String& initial,
                     std::function<void (const juce::String&)> onOk)
{
    auto* w = new juce::AlertWindow (title, {}, juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("texto", initial);
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, onOk] (int result)
    {
        if (result == 1)
            onOk (w->getTextEditorContents ("texto").trim());
    }), true);
}

} // namespace bb
