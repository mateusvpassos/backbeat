#pragma once

#include <cstdint>

namespace bb
{

// Mensagem MIDI curta (até 3 bytes) com o horário de chegada.
// timeSec usa a mesma base de juce::Time::getMillisecondCounterHiRes() * 0.001,
// que é a base dos timestamps de juce::MidiInput.
struct MidiEvent
{
    std::uint8_t data[3] {};
    std::uint8_t size = 0;
    double timeSec = 0.0;

    // Só na fila de monitoramento: nota/valor depois do mapeamento (-1 = descartado pelo perfil).
    std::int16_t mapped = -1;
};

// Mensagem MIDI numa posição em amostras (gravação e reprodução de takes).
struct TimedMidi
{
    std::uint64_t samplePos = 0;
    std::uint8_t data[3] {};
    std::uint8_t size = 0;
};

} // namespace bb
