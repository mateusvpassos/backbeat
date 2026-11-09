#include "mapping/DrumCatalog.h"

#include <map>
#include <string>

namespace bb
{

namespace
{
    const juce::StringArray groupOrder = []
    {
        juce::StringArray a;
        for (const auto* g : { L"Bumbo", L"Caixa", L"Tons", L"Chimbal", L"Condução", L"Ataque", L"Splash", L"China", L"Outros" })
            a.add (juce::String (g));
        return a;
    }();

    juce::String groupForInstrument (const juce::String& instrumentName)
    {
        const auto n = instrumentName.trim();
        if (n.startsWithIgnoreCase ("Kick"))   return "Bumbo";
        if (n.startsWithIgnoreCase ("Snare"))  return "Caixa";
        if (n.startsWithIgnoreCase ("Tom"))    return "Tons";
        if (n.startsWithIgnoreCase ("Hi-Hat") || n.startsWithIgnoreCase ("Hihat")) return "Chimbal";
        if (n.startsWithIgnoreCase ("Ride"))   return L"Condução";
        if (n.startsWithIgnoreCase ("Crash"))  return "Ataque";
        if (n.startsWithIgnoreCase ("Splash")) return "Splash";
        if (n.startsWithIgnoreCase ("China"))  return "China";
        return "Outros";
    }

    // "Tom" + "Tom Lwg VCL 12" → "Tom 12\""; "Crash Edge" + "Crash L Zld 18" → "Crash L Edge"
    juce::String displayName (const juce::String& articulation, const juce::String& instrumentName)
    {
        const auto inst = instrumentName.trim();
        auto tokens = juce::StringArray::fromTokens (inst, " ", "");
        tokens.removeEmptyStrings();

        if (inst.startsWithIgnoreCase ("Tom") && ! tokens.isEmpty() && tokens[tokens.size() - 1].containsOnly ("0123456789"))
        {
            const auto size = tokens[tokens.size() - 1] + "\"";
            return articulation.startsWithIgnoreCase ("Tom")
                       ? "Tom " + size + articulation.substring (3)
                       : articulation + " " + size;
        }

        if (tokens.size() >= 2 && (tokens[1] == "L" || tokens[1] == "R"))
        {
            const auto firstWord = articulation.upToFirstOccurrenceOf (" ", false, false);
            return firstWord + " " + tokens[1] + articulation.fromFirstOccurrenceOf (firstWord, false, false);
        }

        return articulation;
    }
}

void DrumCatalog::add (int note, const juce::String& name, const juce::String& group)
{
    pieces.add ({ note, name, group });
}

void DrumCatalog::sortByGroup()
{
    std::stable_sort (pieces.begin(), pieces.end(), [] (const DrumPiece& a, const DrumPiece& b)
    {
        const auto ga = groupOrder.indexOf (a.group), gb = groupOrder.indexOf (b.group);
        return ga < gb;
    });
}

DrumCatalog DrumCatalog::builtInSsd5()
{
    DrumCatalog c;
    c.sourceDescription = "tabela embutida (Deluxe 2 Free Edition)";

    c.add (36, "Kick", "Bumbo");                    c.add (35, "Kick Double", "Bumbo");
    c.add (38, "Snare Center", "Caixa");            c.add (40, "Snare Rimshot", "Caixa");
    c.add (33, "Snare Rimshot Edge", "Caixa");      c.add (34, "Snare Side", "Caixa");
    c.add (37, "Snare Sidestick", "Caixa");         c.add (39, "Snare Rimclick", "Caixa");
    c.add (21, "Snare Rolls", "Caixa");
    c.add (48, "Tom 10\"", "Tons");                 c.add (19, "Tom 10\" Rimclick", "Tons");
    c.add (47, "Tom 12\"", "Tons");                 c.add (17, "Tom 12\" Rimclick", "Tons");
    c.add (43, "Tom 14\"", "Tons");                 c.add (14, "Tom 14\" Rimclick", "Tons");
    c.add (41, "Tom 16\"", "Tons");                 c.add (12, "Tom 16\" Rimclick", "Tons");
    c.add (42, "Hi-Hat Tip Closed eD", "Chimbal");  c.add (22, "Hi-Hat Shank Closed eD", "Chimbal");
    c.add (44, "Hi-Hat Pedal eD", "Chimbal");
    c.add (60, "Hi-Hat Shank Closed", "Chimbal");   c.add (61, "Hi-Hat Shank 1/5 Open", "Chimbal");
    c.add (62, "Hi-Hat Shank 2/5 Open", "Chimbal"); c.add (63, "Hi-Hat Shank 3/5 Open", "Chimbal");
    c.add (64, "Hi-Hat Shank 4/5 Open", "Chimbal"); c.add (65, "Hi-Hat Shank Open", "Chimbal");
    c.add (66, "Hi-Hat Tip Closed", "Chimbal");     c.add (67, "Hi-Hat Tip 1/5 Open", "Chimbal");
    c.add (68, "Hi-Hat Tip 2/5 Open", "Chimbal");   c.add (69, "Hi-Hat Tip 3/5 Open", "Chimbal");
    c.add (70, "Hi-Hat Tip 4/5 Open", "Chimbal");   c.add (71, "Hi-Hat Tip Open", "Chimbal");
    c.add (72, "Hi-Hat Pedal", "Chimbal");          c.add (73, "Hi-Hat Footsplash", "Chimbal");
    c.add (51, "Ride Bow Tip", L"Condução");         c.add (52, "Ride Bow Shank", L"Condução");
    c.add (53, "Ride Bell", L"Condução");            c.add (59, "Ride Edge", L"Condução");
    c.add (58, "Ride Choke", L"Condução");
    c.add (55, "Crash L Edge", "Ataque");           c.add (91, "Crash L Bow", "Ataque");
    c.add (54, "Crash L Choke", "Ataque");
    c.add (57, "Crash R Edge", "Ataque");           c.add (92, "Crash R Bow", "Ataque");
    c.add (56, "Crash R Choke", "Ataque");
    c.add (50, "Splash Edge", "Splash");            c.add (90, "Splash Bow", "Splash");
    c.add (49, "Splash Choke", "Splash");
    c.add (31, "China Edge", "China");              c.add (93, "China Bow", "China");
    c.add (32, "China Choke", "China");
    return c;
}

std::optional<DrumCatalog> DrumCatalog::fromSsd5KitText (const juce::String& text)
{
    // O .ssd5kit tem um XML de uma linha só, com atributos como:
    //   SamplerInstrumentInfo_Idx_6m_instrumentName="Hi-Hat Sbn 14 AAM"
    //   SamplerInstrumentInfo_Idx_6ArticulationInfo_Idx_15m_nRemappedNoteOn="42"
    //   SamplerInstrumentInfo_Idx_6ArticulationInfo_Idx_15m_name="Hi-Hat Tip Closed eD"
    std::map<int, juce::String> instrumentNames;
    std::map<std::pair<int, int>, std::pair<int, juce::String>> articulations; // (inst, art) → (nota, nome)

    // std::string: o juce::String guarda UTF-8 e cada indexOf/substring percorre desde o início,
    // o que deixava a leitura de um kit (centenas de KB) em ~14 s. Aqui o acesso é direto.
    const std::string s = text.toStdString();
    const std::string prefix = "SamplerInstrumentInfo_Idx_";
    size_t pos = 0;

    while ((pos = s.find (prefix, pos)) != std::string::npos)
    {
        pos += prefix.size();
        const size_t eq = s.find ('=', pos);
        if (eq == std::string::npos || eq - pos > 120 || eq + 1 >= s.size() || s[eq + 1] != '"')
            continue;

        const size_t endQuote = s.find ('"', eq + 2);
        if (endQuote == std::string::npos)
            break;

        const auto key = juce::String (s.substr (pos, eq - pos));                 // ex.: 6ArticulationInfo_Idx_15m_name
        const auto value = juce::String (s.substr (eq + 2, endQuote - (eq + 2)));
        pos = endQuote + 1;

        const int inst = key.getIntValue();
        const auto afterInst = key.trimCharactersAtStart ("0123456789");

        if (afterInst == "m_instrumentName")
        {
            instrumentNames[inst] = value;
        }
        else if (afterInst.startsWith ("ArticulationInfo_Idx_"))
        {
            const auto rest = afterInst.fromFirstOccurrenceOf ("ArticulationInfo_Idx_", false, false);
            const int art = rest.getIntValue();
            const auto field = rest.trimCharactersAtStart ("0123456789");

            auto& entry = articulations[{ inst, art }];
            if (field == "m_nRemappedNoteOn")
                entry.first = value.getIntValue();
            else if (field == "m_name")
                entry.second = value;
        }
    }

    DrumCatalog c;
    for (const auto& [ids, data] : articulations)
    {
        const auto& [note, name] = data;
        if (name.isEmpty() || ! juce::isPositiveAndBelow (note, 128))
            continue;

        const auto inst = instrumentNames.count (ids.first) ? instrumentNames[ids.first] : name;
        c.add (note, displayName (name, inst), groupForInstrument (inst));
    }

    if (c.pieces.isEmpty())
        return std::nullopt;

    c.sortByGroup();
    return c;
}

std::optional<DrumCatalog> DrumCatalog::fromSsd5KitFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return std::nullopt;

    juce::MemoryBlock data;
    if (! file.loadFileAsData (data))
        return std::nullopt;

    // Arquivo com cabeçalho binário + XML em texto: fica só com ASCII imprimível (os nomes são ASCII).
    juce::String text;
    {
        std::string ascii (static_cast<const char*> (data.getData()), data.getSize());
        for (auto& ch : ascii)
            if ((unsigned char) ch < 32 || (unsigned char) ch >= 127)
                ch = ' ';
        text = juce::String (ascii);
    }

    auto catalog = fromSsd5KitText (text);
    if (catalog)
        catalog->sourceDescription = "kit: " + file.getFileNameWithoutExtension();
    return catalog;
}

juce::File DrumCatalog::defaultSsd5KitFile()
{
    return juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory)
        .getChildFile ("SSD5Library/DrumKitPresets/SSD 5 Deluxe 2.lib/01 Rock Kits.typ/Deluxe 2 Free Edition.ssd5kit");
}

const DrumPiece* DrumCatalog::find (int note) const noexcept
{
    for (const auto& p : pieces)
        if (p.note == note)
            return &p;
    return nullptr;
}

juce::String DrumCatalog::nameFor (int note) const
{
    if (const auto* p = find (note))
        return p->name;
    return "nota " + juce::String (note);
}

juce::StringArray DrumCatalog::groupsInOrder() const
{
    juce::StringArray groups;
    for (const auto& p : pieces)
        groups.addIfNotAlreadyThere (p.group);
    return groups;
}

} // namespace bb
