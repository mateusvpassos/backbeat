#include "engine/MidiInputRouter.h"

#include "engine/AudioEngine.h"

namespace bb
{

MidiInputRouter::MidiInputRouter (AudioEngine& e) : engine (e) {}

MidiInputRouter::~MidiInputRouter()
{
    close();
}

juce::Array<juce::MidiDeviceInfo> MidiInputRouter::availableDevices()
{
    return juce::MidiInput::getAvailableDevices();
}

juce::String MidiInputRouter::findPreferredDevice (const juce::String& savedIdentifier)
{
    const auto devices = availableDevices();

    for (const auto& d : devices)
        if (savedIdentifier.isNotEmpty() && d.identifier == savedIdentifier)
            return d.identifier;

    for (const auto& d : devices)
        if (d.name.containsIgnoreCase ("EDRUM"))
            return d.identifier;

    return devices.isEmpty() ? juce::String() : devices.getFirst().identifier;
}

bool MidiInputRouter::open (const juce::String& identifier)
{
    close();

    if (identifier.isEmpty())
        return false;

    input = juce::MidiInput::openDevice (identifier, this);
    if (input == nullptr)
        return false;

    openInfo = input->getDeviceInfo();
    input->start();
    return true;
}

void MidiInputRouter::close()
{
    if (input != nullptr)
    {
        input->stop();
        input.reset();
    }

    openInfo = {};
}

void MidiInputRouter::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    const auto size = m.getRawDataSize();
    const auto* raw = m.getRawData();

    // Só mensagens de canal (nota, CC, aftertouch, etc.). Ignora SysEx, clock, active sensing.
    if (size < 1 || size > 3 || raw[0] < 0x80 || raw[0] >= 0xF0)
        return;

    MidiEvent e;
    e.size = (std::uint8_t) size;
    for (int i = 0; i < size; ++i)
        e.data[i] = raw[i];

    // O timestamp do MidiInput no Windows já vem na base de getMillisecondCounterHiRes()/1000;
    // se vier zerado, usa o horário atual.
    e.timeSec = m.getTimeStamp() > 0.0 ? m.getTimeStamp() : juce::Time::getMillisecondCounterHiRes() * 0.001;

    MidiEvent out = e;
    const bool keep = mapping.load().apply (out);

    if (keep)
        engine.pushMidi (out);

    // Monitor recebe a mensagem crua (para aprender a nota do pad) e o resultado do mapeamento.
    e.mapped = keep ? (std::int16_t) (size >= 3 ? ((raw[0] & 0xF0) == 0xB0 ? out.data[2] : out.data[1]) : 0)
                    : (std::int16_t) -1;
    monitorQueue.push (e); // se a UI estiver atrasada e a fila encher, só perde a exibição
}

} // namespace bb
