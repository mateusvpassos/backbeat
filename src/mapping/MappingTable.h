#pragma once

#include "engine/MidiEvent.h"

#include <array>
#include <cstdint>

namespace bb
{

// Versão "compilada" de um perfil, para usar na thread MIDI: só tabelas fixas, sem alocação.
struct MappingTable
{
    static constexpr std::uint8_t ignore = 255;
    static constexpr std::uint8_t hihatOutputCc = 4; // o SSD5 lê a abertura do chimbal no CC4

    std::array<std::uint8_t, 128> noteTarget {};  // nota de entrada → nota do SSD5 (ou ignore)
    std::array<std::uint8_t, 128> velocity {};    // curva de força
    std::array<std::uint8_t, 128> hihatValue {};  // calibração/inversão do pedal
    std::uint8_t hihatInputCc = 4;

    static MappingTable identity() noexcept
    {
        MappingTable t;
        for (int i = 0; i < 128; ++i)
        {
            t.noteTarget[(size_t) i] = (std::uint8_t) i;
            t.velocity[(size_t) i] = (std::uint8_t) i;
            t.hihatValue[(size_t) i] = (std::uint8_t) i;
        }
        return t;
    }

    // Aplica o mapeamento no lugar. Devolve false se a mensagem deve ser descartada.
    bool apply (MidiEvent& e) const noexcept
    {
        if (e.size < 1)
            return false;

        const std::uint8_t type = e.data[0] & 0xF0;

        if ((type == 0x80 || type == 0x90 || type == 0xA0) && e.size >= 3)
        {
            const auto target = noteTarget[e.data[1] & 0x7F];
            if (target == ignore)
                return false;

            e.data[1] = target;

            if (type == 0x90 && e.data[2] > 0)
                e.data[2] = velocity[e.data[2] & 0x7F] < 1 ? 1 : velocity[e.data[2] & 0x7F];

            return true;
        }

        if (type == 0xB0 && e.size >= 3 && e.data[1] == hihatInputCc)
        {
            e.data[1] = hihatOutputCc;
            e.data[2] = hihatValue[e.data[2] & 0x7F];
        }

        return true;
    }
};

} // namespace bb
