#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include "engine/AudioEngine.h"

#include <functional>

namespace bb
{

// Mixagem de um take em um único WAV (F6), fora do tempo real.
//
// Junta bateria.wav, chrome.wav e entrada-N.wav (todas começam na mesma amostra e têm o mesmo tamanho)
// com os volumes do mixer, e grava <take>/mixagem.wav (estéreo, 24 bits). Se a soma passar de 0 dBFS,
// a mixagem inteira é abaixada para o pico ficar em -1 dBFS (nada de clipar).
namespace mixexport
{
    // Arquivo de cada faixa do mixer dentro da pasta do take.
    juce::String trackFileName (int track);

    struct Gains
    {
        float track[numTracks] { 1.0f, 1.0f, 1.0f, 1.0f }; // já com mudo/solo aplicados (0 = fora)
        float master = 1.0f;
    };

    struct Result
    {
        bool ok = false;
        bool cancelled = false;
        juce::String error;
        juce::File file;
        int tracksUsed = 0;
        double seconds = 0.0;
        float peak = 0.0f;          // pico da soma antes de qualquer ajuste
        float appliedGain = 1.0f;   // < 1 quando foi preciso abaixar para não clipar
    };

    // `progress` (opcional) recebe 0..1 e devolve false para cancelar. Roda em qualquer thread.
    Result exportTake (const juce::File& takeFolder, const Gains& gains,
                       const std::function<bool (float)>& progress = {},
                       const juce::String& outputName = "mixagem.wav");
}

} // namespace bb
