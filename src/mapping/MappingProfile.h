#pragma once

#include <juce_core/juce_core.h>

#include "mapping/MappingTable.h"

namespace bb
{

enum class Command
{
    none = 0,
    record,   // gravar
    play,     // tocar
    stop,     // parar
    panic     // pânico (para todas as notas)
};

juce::String commandId (Command c);          // "gravar", "tocar"...
juce::String commandLabel (Command c);       // "Gravar", "Tocar"...
Command commandFromId (const juce::String&);
const juce::Array<Command>& allCommands();

// Perfil de mapeamento: pads da bateria → peças do SSD5, curva de força, pedal do chimbal e teclas do PC.
// Fica em JSON em <dados>/perfis/<nome>.json.
struct MappingProfile
{
    struct NoteRule
    {
        int input = 0;   // nota que a bateria manda
        int output = 0;  // nota do SSD5 (peça); -1 = ignorar
    };

    struct KeyBinding
    {
        juce::String key;          // descrição do juce::KeyPress, ex.: "Z", "F9", "ctrl + R"
        int note = -1;             // peça (>= 0) ...
        Command command = Command::none; // ... ou comando
        int velocity = 100;
    };

    juce::String name { "Novo perfil" };
    juce::Array<NoteRule> notes;
    bool passUnmappedNotes = true;     // notas fora da lista: tocar como chegam (true) ou ignorar

    double velocityCurve = 0.0;        // -1 (mais suave) .. +1 (mais forte)
    int velocityMin = 1;               // força mínima de saída

    int hihatCc = 4;                   // CC do pedal do chimbal na bateria
    bool hihatInvert = false;
    int hihatMin = 0, hihatMax = 127;  // faixa real do pedal (calibração)

    juce::Array<KeyBinding> keys;
    bool globalCommandKeys = true;     // comandos funcionam com outra janela em foco (Songsterr)

    // ---- edição ----
    int indexOfInput (int inputNote) const;
    void setRule (int inputNote, int outputNote); // cria ou atualiza, mantém ordenado por entrada
    void removeRule (int inputNote);

    // ---- conversões ----
    MappingTable compile() const;
    static int curveVelocity (int velocity, double curve, int minimum);

    juce::var toVar() const;
    static MappingProfile fromVar (const juce::var&);

    // Perfil inicial: Nexus mandando General MIDI → notas do SSD5 (corrige tons e pratos que diferem).
    static MappingProfile defaultNexusGm();
};

} // namespace bb
