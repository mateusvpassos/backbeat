#include "rec/TakeFiles.h"

#include <algorithm>
#include <map>

namespace bb::takefiles
{

namespace
{
    bool isNoteOn (const TakeEvent& e)  { return e.size >= 3 && (e.data[0] & 0xF0) == 0x90 && e.data[2] > 0; }
    bool isNoteOff (const TakeEvent& e) { return e.size >= 3 && ((e.data[0] & 0xF0) == 0x80 || ((e.data[0] & 0xF0) == 0x90 && e.data[2] == 0)); }
    int key (const TakeEvent& e)        { return ((e.data[0] & 0x0F) << 8) | e.data[1]; }

    void sortByTime (std::vector<TakeEvent>& events)
    {
        // Mesmo instante: note-off antes de note-on, para não cortar a nota nova.
        std::stable_sort (events.begin(), events.end(), [] (const TakeEvent& a, const TakeEvent& b)
        {
            if (a.timeSec != b.timeSec)
                return a.timeSec < b.timeSec;
            return isNoteOff (a) && ! isNoteOff (b);
        });
    }
}

void addMissingNoteOffs (std::vector<TakeEvent>& events, double gateSec)
{
    sortByTime (events);

    std::map<int, size_t> open; // canal/nota → índice do note-on sem note-off
    std::vector<TakeEvent> extra;

    const auto closeAt = [&] (const TakeEvent& on, double t)
    {
        TakeEvent off;
        off.size = 3;
        off.data[0] = (std::uint8_t) (0x80 | (on.data[0] & 0x0F));
        off.data[1] = on.data[1];
        off.data[2] = 0;
        off.timeSec = t;
        extra.push_back (off);
    };

    for (size_t i = 0; i < events.size(); ++i)
    {
        const auto& e = events[i];

        if (isNoteOn (e))
        {
            if (auto it = open.find (key (e)); it != open.end())
            {
                // Nova batida na mesma nota sem ter fechado a anterior.
                const auto& prev = events[it->second];
                closeAt (prev, juce::jmin (prev.timeSec + gateSec, e.timeSec - 0.0001));
            }
            open[key (e)] = i;
        }
        else if (isNoteOff (e))
        {
            if (auto it = open.find (key (e)); it != open.end())
            {
                // Note-off colado no note-on (duração ~0): estica até o gate.
                auto& off = events[i];
                const double start = events[it->second].timeSec;
                if (off.timeSec - start < 0.005)
                    off.timeSec = start + juce::jmin (gateSec, 0.02);
                open.erase (it);
            }
        }
    }

    for (const auto& [k, index] : open)
        closeAt (events[index], events[index].timeSec + gateSec);

    events.insert (events.end(), extra.begin(), extra.end());
    sortByTime (events);
}

bool writeMidiFile (const juce::File& file, std::vector<TakeEvent> events, double bpm, const juce::String& trackName)
{
    addMissingNoteOffs (events);

    const double ticksPerSecond = ticksPerQuarter * bpm / 60.0;
    juce::MidiMessageSequence track;

    auto name = juce::MidiMessage::textMetaEvent (3, trackName);
    name.setTimeStamp (0);
    track.addEvent (name);

    auto tempo = juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60'000'000.0 / bpm));
    tempo.setTimeStamp (0);
    track.addEvent (tempo);

    auto signature = juce::MidiMessage::timeSignatureMetaEvent (4, 4);
    signature.setTimeStamp (0);
    track.addEvent (signature);

    double lastTick = 0;
    for (const auto& e : events)
    {
        auto m = juce::MidiMessage (e.data, (int) e.size);
        const double tick = std::round (e.timeSec * ticksPerSecond);
        m.setTimeStamp (tick);
        track.addEvent (m);
        lastTick = juce::jmax (lastTick, tick);
    }

    auto end = juce::MidiMessage::endOfTrack();
    end.setTimeStamp (lastTick + 1);
    track.addEvent (end);
    track.sort();
    track.updateMatchedPairs();

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote (ticksPerQuarter);
    midi.addTrack (track);

    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk())
        return false;

    return midi.writeTo (out, 1);
}

std::vector<TakeEvent> readMidiFile (const juce::File& file)
{
    std::vector<TakeEvent> result;

    juce::FileInputStream in (file);
    if (! in.openedOk())
        return result;

    juce::MidiFile midi;
    if (! midi.readFrom (in))
        return result;

    midi.convertTimestampTicksToSeconds();

    for (int t = 0; t < midi.getNumTracks(); ++t)
        for (const auto* holder : *midi.getTrack (t))
        {
            const auto& m = holder->message;
            const auto size = m.getRawDataSize();
            if (size < 1 || size > 3 || m.getRawData()[0] < 0x80 || m.getRawData()[0] >= 0xF0)
                continue;

            TakeEvent e;
            e.timeSec = m.getTimeStamp();
            e.size = (std::uint8_t) size;
            for (int i = 0; i < size; ++i)
                e.data[i] = m.getRawData()[i];
            result.push_back (e);
        }

    sortByTime (result);
    return result;
}

} // namespace bb::takefiles
