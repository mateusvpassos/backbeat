#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "engine/MidiEvent.h"
#include "mapping/MappingTable.h"
#include "util/SeqLock.h"
#include "util/SpscQueue.h"

#include <functional>
#include <memory>

namespace bb
{

class AudioEngine;

// Abre a entrada MIDI (a EDRUM) e repassa as mensagens:
//   - para a fila do AudioEngine (tocar o plugin);
//   - para uma fila de monitoramento que a UI lê (mostrar a última nota).
// O callback roda na thread MIDI do JUCE; aqui também não há alocação nem lock.
class MidiInputRouter final : private juce::MidiInputCallback
{
public:
    explicit MidiInputRouter (AudioEngine& engine);
    ~MidiInputRouter() override;

    // Lista e abertura: thread de mensagens.
    static juce::Array<juce::MidiDeviceInfo> availableDevices();
    static juce::String findPreferredDevice (const juce::String& savedIdentifier); // salvo → "EDRUM" → primeiro

    bool open (const juce::String& identifier);
    void close();

    juce::String getOpenIdentifier() const { return openInfo.identifier; }
    juce::String getOpenName() const       { return openInfo.name; }
    bool isOpen() const noexcept           { return input != nullptr; }

    // Fila de monitoramento (consumida pela UI): mensagem crua + resultado do mapeamento em `mapped`.
    bool popMonitorEvent (MidiEvent& e) noexcept { return monitorQueue.pop (e); }

    // Troca o mapeamento ativo (thread de mensagens). A thread MIDI lê sem lock.
    void setMapping (const MappingTable& table) noexcept { mapping.store (table); }

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    AudioEngine& engine;
    SeqLock<MappingTable> mapping { MappingTable::identity() };
    std::unique_ptr<juce::MidiInput> input;
    juce::MidiDeviceInfo openInfo;
    SpscQueue<MidiEvent, 256> monitorQueue;
};

} // namespace bb
