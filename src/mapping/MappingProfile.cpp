#include "mapping/MappingProfile.h"

#include <cmath>

namespace bb
{

// ---------------------------------------------------------------------------
// Comandos
// ---------------------------------------------------------------------------

juce::String commandId (Command c)
{
    switch (c)
    {
        case Command::record: return "gravar";
        case Command::play:   return "tocar";
        case Command::stop:   return "parar";
        case Command::panic:  return "panico";
        case Command::none:   break;
    }
    return {};
}

juce::String commandLabel (Command c)
{
    switch (c)
    {
        case Command::record: return "Gravar";
        case Command::play:   return "Tocar";
        case Command::stop:   return "Parar";
        case Command::panic:  return L"Pânico (cortar som)";
        case Command::none:   break;
    }
    return {};
}

Command commandFromId (const juce::String& id)
{
    for (auto c : allCommands())
        if (commandId (c) == id)
            return c;
    return Command::none;
}

const juce::Array<Command>& allCommands()
{
    static const juce::Array<Command> list { Command::record, Command::play, Command::stop, Command::panic };
    return list;
}

// ---------------------------------------------------------------------------
// Edição
// ---------------------------------------------------------------------------

int MappingProfile::indexOfInput (int inputNote) const
{
    for (int i = 0; i < notes.size(); ++i)
        if (notes.getReference (i).input == inputNote)
            return i;
    return -1;
}

void MappingProfile::setRule (int inputNote, int outputNote)
{
    if (const int i = indexOfInput (inputNote); i >= 0)
    {
        notes.getReference (i).output = outputNote;
        return;
    }

    int insertAt = 0;
    while (insertAt < notes.size() && notes.getReference (insertAt).input < inputNote)
        ++insertAt;

    notes.insert (insertAt, { inputNote, outputNote });
}

void MappingProfile::removeRule (int inputNote)
{
    if (const int i = indexOfInput (inputNote); i >= 0)
        notes.remove (i);
}

// ---------------------------------------------------------------------------
// Compilação para a thread MIDI
// ---------------------------------------------------------------------------

int MappingProfile::curveVelocity (int velocity, double curve, int minimum)
{
    if (velocity <= 0)
        return 0;

    minimum = juce::jlimit (1, 127, minimum);
    curve = juce::jlimit (-1.0, 1.0, curve);

    // curva > 0 → expoente < 1 → toques fracos ficam mais fortes.
    const double exponent = std::pow (2.0, -curve * 1.5);
    const double x = juce::jlimit (0.0, 1.0, velocity / 127.0);
    const double y = minimum + (127.0 - minimum) * std::pow (x, exponent);
    return juce::jlimit (1, 127, (int) std::lround (y));
}

MappingTable MappingProfile::compile() const
{
    MappingTable t = MappingTable::identity();

    if (! passUnmappedNotes)
        t.noteTarget.fill (MappingTable::ignore);

    for (const auto& rule : notes)
        if (juce::isPositiveAndBelow (rule.input, 128))
            t.noteTarget[(size_t) rule.input] = juce::isPositiveAndBelow (rule.output, 128)
                                                    ? (std::uint8_t) rule.output
                                                    : MappingTable::ignore;

    for (int v = 0; v < 128; ++v)
        t.velocity[(size_t) v] = (std::uint8_t) curveVelocity (v, velocityCurve, velocityMin);

    const int lo = juce::jlimit (0, 126, juce::jmin (hihatMin, hihatMax));
    const int hi = juce::jlimit (lo + 1, 127, juce::jmax (hihatMin, hihatMax));

    for (int v = 0; v < 128; ++v)
    {
        const double x = juce::jlimit (0.0, 1.0, (v - lo) / (double) (hi - lo));
        const int out = (int) std::lround ((hihatInvert ? 1.0 - x : x) * 127.0);
        t.hihatValue[(size_t) v] = (std::uint8_t) juce::jlimit (0, 127, out);
    }

    t.hihatInputCc = (std::uint8_t) juce::jlimit (0, 127, hihatCc);
    return t;
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------

juce::var MappingProfile::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("versao", 1);
    root->setProperty ("nome", name);
    root->setProperty ("notasForaDaLista", passUnmappedNotes ? "tocar" : "ignorar");

    juce::Array<juce::var> noteList;
    for (const auto& r : notes)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("entrada", r.input);
        o->setProperty ("saida", r.output);
        noteList.add (juce::var (o));
    }
    root->setProperty ("notas", noteList);

    auto* vel = new juce::DynamicObject();
    vel->setProperty ("curva", velocityCurve);
    vel->setProperty ("minimo", velocityMin);
    root->setProperty ("forca", juce::var (vel));

    auto* hh = new juce::DynamicObject();
    hh->setProperty ("cc", hihatCc);
    hh->setProperty ("inverter", hihatInvert);
    hh->setProperty ("min", hihatMin);
    hh->setProperty ("max", hihatMax);
    root->setProperty ("chimbal", juce::var (hh));

    juce::Array<juce::var> keyList;
    for (const auto& k : keys)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("tecla", k.key);
        if (k.command != Command::none)
        {
            o->setProperty ("comando", commandId (k.command));
        }
        else
        {
            o->setProperty ("nota", k.note);
            o->setProperty ("forca", k.velocity);
        }
        keyList.add (juce::var (o));
    }
    root->setProperty ("teclas", keyList);
    root->setProperty ("atalhosGlobais", globalCommandKeys);

    return juce::var (root);
}

MappingProfile MappingProfile::fromVar (const juce::var& v)
{
    MappingProfile p;
    p.name = v.getProperty ("nome", p.name).toString();
    p.passUnmappedNotes = v.getProperty ("notasForaDaLista", "tocar").toString() != "ignorar";

    if (auto* list = v.getProperty ("notas", {}).getArray())
        for (const auto& item : *list)
        {
            const int in = item.getProperty ("entrada", -1);
            const int out = item.getProperty ("saida", -1);
            if (juce::isPositiveAndBelow (in, 128))
                p.setRule (in, juce::isPositiveAndBelow (out, 128) ? out : -1);
        }

    const auto vel = v.getProperty ("forca", {});
    p.velocityCurve = juce::jlimit (-1.0, 1.0, (double) vel.getProperty ("curva", 0.0));
    p.velocityMin = juce::jlimit (1, 127, (int) vel.getProperty ("minimo", 1));

    const auto hh = v.getProperty ("chimbal", {});
    p.hihatCc = juce::jlimit (0, 127, (int) hh.getProperty ("cc", 4));
    p.hihatInvert = (bool) hh.getProperty ("inverter", false);
    p.hihatMin = juce::jlimit (0, 127, (int) hh.getProperty ("min", 0));
    p.hihatMax = juce::jlimit (0, 127, (int) hh.getProperty ("max", 127));

    if (auto* list = v.getProperty ("teclas", {}).getArray())
        for (const auto& item : *list)
        {
            KeyBinding k;
            k.key = item.getProperty ("tecla", {}).toString();
            k.command = commandFromId (item.getProperty ("comando", {}).toString());
            k.note = item.getProperty ("nota", -1);
            k.velocity = juce::jlimit (1, 127, (int) item.getProperty ("forca", 100));

            const bool valid = k.key.isNotEmpty()
                            && (k.command != Command::none || juce::isPositiveAndBelow (k.note, 128));
            if (valid)
                p.keys.add (k);
        }

    p.globalCommandKeys = (bool) v.getProperty ("atalhosGlobais", true);
    return p;
}

MappingProfile MappingProfile::defaultNexusGm()
{
    MappingProfile p;
    p.name = "Nexus (General MIDI)";

    // General MIDI → SSD5 Deluxe 2. Onde as notas coincidem, mantém.
    p.setRule (36, 36); // bumbo
    p.setRule (38, 38); // caixa centro
    p.setRule (40, 40); // caixa rimshot
    p.setRule (37, 37); // aro (cross stick)
    p.setRule (42, 42); // chimbal fechado → Tip eD (o SSD5 abre pelo CC4)
    p.setRule (46, 42); // chimbal aberto  → Tip eD (a abertura vem do pedal)
    p.setRule (44, 44); // pedal do chimbal → Pedal eD
    p.setRule (50, 48); // tom agudo (GM 50 no SSD5 é o splash!) → Tom 10
    p.setRule (48, 48); // tom → Tom 10
    p.setRule (47, 47); // tom → Tom 12
    p.setRule (45, 43); // tom grave → Tom 14
    p.setRule (43, 41); // surdo → Tom 16
    p.setRule (41, 41); // surdo grave → Tom 16
    p.setRule (49, 55); // ataque 1 → Crash L Edge
    p.setRule (57, 57); // ataque 2 → Crash R Edge
    p.setRule (55, 50); // splash → Splash Edge
    p.setRule (52, 31); // china → China Edge
    p.setRule (51, 51); // condução → Ride Bow Tip
    p.setRule (59, 59); // condução 2 → Ride Edge
    p.setRule (53, 53); // cúpula → Ride Bell

    // Teclado do PC (com o Backbeat em foco): fileira de baixo = bateria básica, de cima = pratos/tons.
    const auto piece = [&p] (const char* key, int note) { p.keys.add ({ key, note, Command::none, 100 }); };
    piece ("Z", 36); piece ("X", 38); piece ("C", 66); piece ("V", 71); piece ("B", 72);
    piece ("A", 48); piece ("S", 47); piece ("D", 43); piece ("F", 41);
    piece ("Q", 55); piece ("W", 57); piece ("E", 51); piece ("R", 53);

    // Comandos globais (funcionam com o Songsterr na frente).
    p.keys.add ({ "F9", -1, Command::record, 100 });
    p.keys.add ({ "F10", -1, Command::play, 100 });
    p.keys.add ({ "F8", -1, Command::stop, 100 });
    return p;
}

} // namespace bb
