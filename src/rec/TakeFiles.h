#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

namespace bb
{

// Evento de um take, em segundos desde o início da gravação.
struct TakeEvent
{
    double timeSec = 0.0;
    std::uint8_t data[3] {};
    std::uint8_t size = 0;
};

namespace takefiles
{
    constexpr int ticksPerQuarter = 960;

    // Baterias eletrônicas às vezes não mandam note-off (ou mandam colado no note-on). Programas como o
    // FL Studio mostram notas de duração zero nesse caso: cria um note-off `gateSec` depois (ou antes da
    // próxima batida na mesma nota).
    void addMissingNoteOffs (std::vector<TakeEvent>& events, double gateSec = 0.05);

    // .mid tipo 1 com uma faixa "Bateria": andamento, compasso 4/4 e os eventos (canal preservado).
    bool writeMidiFile (const juce::File& file, std::vector<TakeEvent> events, double bpm, const juce::String& trackName);

    // Lê qualquer .mid e devolve as mensagens de canal em segundos (todas as faixas, em ordem).
    std::vector<TakeEvent> readMidiFile (const juce::File& file);
}

} // namespace bb
