#include <juce_core/juce_core.h>

#include "engine/PluginHost.h"
#include "mapping/DrumCatalog.h"
#include "mapping/MappingProfile.h"
#include "mapping/ProfileManager.h"
#include "util/DrumNames.h"
#include "util/SeqLock.h"
#include "util/Settings.h"
#include "util/SpscQueue.h"

#include <thread>

namespace bb
{

namespace
{
    MidiEvent makeEvent (std::uint8_t status, std::uint8_t d1, std::uint8_t d2)
    {
        MidiEvent e;
        e.size = 3;
        e.data[0] = status;
        e.data[1] = d1;
        e.data[2] = d2;
        return e;
    }
}

class MappingTest final : public juce::UnitTest
{
public:
    MappingTest() : juce::UnitTest ("Mapeamento", "mapping") {}

    void runTest() override
    {
        beginTest (L"perfil padrão: GM da Nexus → SSD5");
        {
            const auto table = MappingProfile::defaultNexusGm().compile();

            auto e = makeEvent (0x99, 38, 100); // caixa
            expect (table.apply (e));
            expectEquals ((int) e.data[1], 38);

            e = makeEvent (0x99, 50, 100);      // GM 50 (tom agudo) não pode virar splash
            expect (table.apply (e));
            expectEquals ((int) e.data[1], 48);

            e = makeEvent (0x89, 50, 0);        // note off segue a mesma nota
            expect (table.apply (e));
            expectEquals ((int) e.data[1], 48);

            e = makeEvent (0x99, 99, 100);      // fora da lista: passa direto (padrão)
            expect (table.apply (e));
            expectEquals ((int) e.data[1], 99);
        }

        beginTest ("ignorar e notas fora da lista");
        {
            MappingProfile p;
            p.setRule (38, -1);
            p.setRule (36, 36);
            p.passUnmappedNotes = false;
            const auto t = p.compile();

            auto e = makeEvent (0x99, 38, 100);
            expect (! t.apply (e), "nota ignorada deve ser descartada");
            e = makeEvent (0x99, 40, 100);
            expect (! t.apply (e), "fora da lista com 'ignorar' deve ser descartada");
            e = makeEvent (0x99, 36, 100);
            expect (t.apply (e));
        }

        beginTest (L"curva de força");
        {
            expectEquals (MappingProfile::curveVelocity (0, 0.5, 1), 0); // note off continua 0
            expectEquals (MappingProfile::curveVelocity (127, -1.0, 1), 127);
            expectEquals (MappingProfile::curveVelocity (64, 0.0, 1), 64); // linear

            expect (MappingProfile::curveVelocity (40, 0.8, 1) > 40, "curva positiva deixa toque fraco mais forte");
            expect (MappingProfile::curveVelocity (40, -0.8, 1) < 40, "curva negativa deixa toque fraco mais fraco");
            expect (MappingProfile::curveVelocity (1, 0.0, 30) >= 30, L"respeita o mínimo");

            int previous = 0;
            bool monotonic = true;
            for (int v = 1; v < 128; ++v)
            {
                const int out = MappingProfile::curveVelocity (v, 0.6, 10);
                monotonic = monotonic && out >= previous;
                previous = out;
            }
            expect (monotonic, "curva precisa ser crescente");
        }

        beginTest (L"pedal do chimbal: CC, faixa e inversão");
        {
            MappingProfile p;
            p.hihatCc = 4;
            p.hihatMin = 20;
            p.hihatMax = 100;
            p.hihatInvert = true;
            const auto t = p.compile();

            auto e = makeEvent (0xB9, 4, 20);
            t.apply (e);
            expectEquals ((int) e.data[1], 4);
            expectEquals ((int) e.data[2], 127); // invertido: mínimo do pedal → 127

            e = makeEvent (0xB9, 4, 110);
            t.apply (e);
            expectEquals ((int) e.data[2], 0);

            e = makeEvent (0xB9, 7, 55);         // outro CC passa intacto
            t.apply (e);
            expectEquals ((int) e.data[1], 7);
            expectEquals ((int) e.data[2], 55);
        }

        beginTest ("JSON ida e volta");
        {
            auto p = MappingProfile::defaultNexusGm();
            p.velocityCurve = 0.25;
            p.hihatInvert = true;
            p.globalCommandKeys = false;

            const auto json = juce::JSON::toString (p.toVar());
            const auto back = MappingProfile::fromVar (juce::JSON::parse (json));

            expectEquals (back.name, p.name);
            expectEquals (back.notes.size(), p.notes.size());
            expectEquals (back.keys.size(), p.keys.size());
            expectEquals (back.velocityCurve, 0.25);
            expect (back.hihatInvert);
            expect (! back.globalCommandKeys);
            expect (back.keys.getReference (back.keys.size() - 1).command == Command::metronome);

            const auto a = p.compile(), b = back.compile();
            expect (a.noteTarget == b.noteTarget && a.velocity == b.velocity && a.hihatValue == b.hihatValue);
        }

        beginTest ("SeqLock");
        {
            SeqLock<MappingTable> lock { MappingTable::identity() };
            expectEquals ((int) lock.load().noteTarget[50], 50);
            lock.store (MappingProfile::defaultNexusGm().compile());
            expectEquals ((int) lock.load().noteTarget[50], 48);
        }
    }
};

class CatalogTest final : public juce::UnitTest
{
public:
    CatalogTest() : juce::UnitTest (L"Catálogo SSD5", "mapping") {}

    void runTest() override
    {
        beginTest ("texto de .ssd5kit");
        {
            const juce::String text =
                "<SAMPLER_Kit SamplerInstrumentInfo_Idx_0m_instrumentName=\"Kick Pearl 22 MMC\" "
                "SamplerInstrumentInfo_Idx_0ArticulationInfo_Idx_0m_nNativeNoteOn=\"36\" "
                "SamplerInstrumentInfo_Idx_0ArticulationInfo_Idx_0m_nRemappedNoteOn=\"36\" "
                "SamplerInstrumentInfo_Idx_0ArticulationInfo_Idx_0m_name=\"Kick\" "
                "SamplerInstrumentInfo_Idx_3m_instrumentName=\"Tom Lwg VCL 12\" "
                "SamplerInstrumentInfo_Idx_3ArticulationInfo_Idx_1m_nRemappedNoteOn=\"17\" "
                "SamplerInstrumentInfo_Idx_3ArticulationInfo_Idx_1m_name=\"Tom Rimclick\" "
                "SamplerInstrumentInfo_Idx_8m_instrumentName=\"Crash L  Zld 18 ACU\" "
                "SamplerInstrumentInfo_Idx_8ArticulationInfo_Idx_0m_nRemappedNoteOn=\"55\" "
                "SamplerInstrumentInfo_Idx_8ArticulationInfo_Idx_0m_name=\"Crash Edge\" />";

            const auto c = DrumCatalog::fromSsd5KitText (text);
            expect (c.has_value());
            if (c)
            {
                expectEquals (c->getPieces().size(), 3);
                expectEquals (c->nameFor (36), juce::String ("Kick"));
                expectEquals (c->nameFor (17), juce::String ("Tom 12\" Rimclick"));
                expectEquals (c->nameFor (55), juce::String ("Crash L Edge"));
                expectEquals (c->find (55)->group, juce::String ("Ataque"));
            }
        }

        beginTest (L"kit instalado (se existir nesta máquina)");
        {
            const auto file = DrumCatalog::defaultSsd5KitFile();
            if (file.existsAsFile())
            {
                const auto c = DrumCatalog::fromSsd5KitFile (file);
                expect (c.has_value());
                if (c)
                {
                    expect (c->getPieces().size() >= 40);
                    expectEquals (c->nameFor (42), juce::String ("Hi-Hat Tip Closed eD"));
                    expectEquals (c->nameFor (38), juce::String ("Snare Center"));

                    // A tabela embutida precisa bater com o kit real.
                    const auto builtIn = DrumCatalog::builtInSsd5();
                    for (const auto& piece : c->getPieces())
                        expect (builtIn.find (piece.note) != nullptr, "faltando na tabela embutida: " + piece.name);
                }
            }
            else
            {
                logMessage (L"kit do SSD5 não encontrado, pulando");
            }
        }
    }
};

class ProfileManagerTest final : public juce::UnitTest
{
public:
    ProfileManagerTest() : juce::UnitTest ("Perfis", "mapping") {}

    void runTest() override
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("backbeat-perfis-" + juce::String (juce::Random::getSystemRandom().nextInt64()));

        beginTest (L"cria padrão, duplica, renomeia, exclui");
        {
            ProfileManager pm (dir);
            int notifications = 0;
            pm.onChanged = [&] { ++notifications; };

            pm.loadOrCreate ({});
            expectEquals (pm.names().size(), 1);
            expectEquals (pm.current().name, juce::String ("Nexus (General MIDI)"));

            pm.createNew (pm.current().name, true);
            expectEquals (pm.names().size(), 2);
            expect (pm.current().name != "Nexus (General MIDI)");
            expectEquals (pm.current().notes.size(), MappingProfile::defaultNexusGm().notes.size());

            expect (pm.rename ("Treino"));
            expect (pm.names().contains ("Treino"));
            expect (! pm.rename ("Nexus (General MIDI)"), "nome repetido deve falhar");

            pm.edit().setRule (26, 22);
            pm.commit();

            ProfileManager again (dir);
            again.loadOrCreate ("Treino");
            expectEquals (again.current().name, juce::String ("Treino"));
            expect (again.current().indexOfInput (26) >= 0);

            expect (again.removeCurrent());
            expectEquals (again.names().size(), 1);
            expect (! again.removeCurrent(), L"o último perfil não pode ser excluído");
            expect (notifications > 0);
        }

        dir.deleteRecursively();
    }
};

class PluginFilesTest final : public juce::UnitTest
{
public:
    PluginFilesTest() : juce::UnitTest ("PluginFiles", "Backbeat") {}

    void runTest() override
    {
        beginTest ("acha .vst3 (arquivo ou pasta) sem entrar nos bundles");
        {
            const auto root = juce::File::createTempFile ("vst3scan");
            root.createDirectory();
            root.getChildFile ("SSDSampler5.vst3/Contents/x86_64-win").createDirectory();
            root.getChildFile ("SSDSampler5.vst3/Contents/x86_64-win/SSDSampler5.vst3").create(); // dentro do bundle
            root.getChildFile ("Fabricante/Drums.vst3").create();
            root.getChildFile ("Fabricante/leiame.txt").create();
            root.getChildFile ("Outro.VST3").createDirectory();

            auto found = PluginHost::findVst3In (root);
            juce::StringArray names;
            for (const auto& f : found)
                names.add (f.getFileName());
            names.sort (true);
            expectEquals (names.joinIntoString (","), juce::String ("Drums.vst3,Outro.VST3,SSDSampler5.vst3"));

            expect (PluginHost::findVst3In (root.getChildFile ("nao-existe")).isEmpty());
            root.deleteRecursively();
        }

        beginTest ("reconhece o SSD5");
        expect (PluginHost::isSsd5 (juce::File ("C:\\x\\SSDSampler5.vst3")));
        expect (! PluginHost::isSsd5 (juce::File ("C:\\x\\MT-PowerDrumKit.vst3")));

        beginTest ("tabela General MIDI para os outros plugins");
        {
            const auto gm = DrumCatalog::builtInGm();
            expect (! gm.isSsd5);
            expect (DrumCatalog::builtInSsd5().isSsd5);
            expect (gm.find (36) != nullptr && gm.find (36)->group == "Bumbo");
            expect (gm.find (46) != nullptr && gm.find (46)->group == "Chimbal");
            expect (gm.find (50) != nullptr && gm.find (50)->group == "Tons");
            expectEquals (gm.groupsInOrder()[0], juce::String ("Bumbo"));
        }
    }
};

static PluginFilesTest pluginFilesTest;

static MappingTest mappingTest;
static CatalogTest catalogTest;
static ProfileManagerTest profileManagerTest;

class SpscQueueTest final : public juce::UnitTest
{
public:
    SpscQueueTest() : juce::UnitTest ("SpscQueue", "util") {}

    void runTest() override
    {
        beginTest ("vazia, cheia e ordem");
        {
            SpscQueue<int, 4> q;
            int v = 0;
            expect (! q.pop (v));
            for (int i = 0; i < 4; ++i)
                expect (q.push (i));
            expect (! q.push (99), "fila cheia deve recusar");
            for (int i = 0; i < 4; ++i)
            {
                expect (q.pop (v));
                expectEquals (v, i);
            }
            expect (! q.pop (v));
        }

        beginTest ("produtor e consumidor em threads separadas");
        {
            SpscQueue<int, 256> q;
            constexpr int total = 200000;

            std::thread producer ([&]
            {
                for (int i = 0; i < total;)
                    if (q.push (i))
                        ++i;
            });

            int expected = 0;
            bool inOrder = true;
            while (expected < total)
            {
                int v;
                if (q.pop (v))
                {
                    inOrder = inOrder && (v == expected);
                    ++expected;
                }
            }

            producer.join();
            expect (inOrder, "itens fora de ordem ou perdidos");
            expectEquals ((int) q.size(), 0);
        }
    }
};

class SettingsTest final : public juce::UnitTest
{
public:
    SettingsTest() : juce::UnitTest ("Settings", "util") {}

    void runTest() override
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("backbeat-test-" + juce::String (juce::Random::getSystemRandom().nextInt64()));

        beginTest ("salvar e carregar");
        {
            Settings s (dir);
            expect (! s.load(), L"sem arquivo, começa vazio");
            expectEquals (s.get ("plugin", "caminho", "padrao").toString(), juce::String ("padrao"));

            s.set ("plugin", "caminho", "C:\\x\\SSDSampler5.vst3");
            s.set ("saida", "volumeBateriaDb", -3.5);
            s.set ("midi", "tempo", "estavel");
            expect (s.save());

            Settings again (dir);
            expect (again.load());
            expectEquals (again.get ("plugin", "caminho").toString(), juce::String ("C:\\x\\SSDSampler5.vst3"));
            expectEquals ((double) again.get ("saida", "volumeBateriaDb"), -3.5);
            expectEquals ((int) again.get ("versao", "x", 0), 0); // "versao" é número, não seção
        }

        beginTest (L"arquivo corrompido não derruba");
        {
            dir.getChildFile ("settings.json").replaceWithText (L"{ isso não é json");
            Settings s (dir);
            expect (! s.load());
            s.set ("a", "b", 1);
            expect (s.save());
        }

        dir.deleteRecursively();
    }
};

class DrumNamesTest final : public juce::UnitTest
{
public:
    DrumNamesTest() : juce::UnitTest ("DrumNames", "util") {}

    void runTest() override
    {
        beginTest ("General MIDI");
        expectEquals (gmDrumName (38), juce::String ("Caixa"));
        expectEquals (gmDrumName (36), juce::String ("Bumbo"));
        expect (gmDrumName (60).isEmpty());
        expect (describeDrumNote (38).startsWith (juce::String (L"38 · D1")));
    }
};

static SpscQueueTest spscQueueTest;
static SettingsTest settingsTest;
static DrumNamesTest drumNamesTest;

} // namespace bb

int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        const auto* r = runner.getResult (i);
        failures += r->failures;
        passes += r->passes;
        std::printf ("%-6s %s / %s (%d ok)\n", r->failures == 0 ? "ok" : "FALHA",
                     r->unitTestName.toRawUTF8(), r->subcategoryName.toRawUTF8(), r->passes);
        for (const auto& message : r->messages)
            std::printf ("       %s\n", message.toRawUTF8());
    }

    std::printf ("\n%s: %d verificacoes, %d falha(s)\n", failures == 0 ? "OK" : "FALHOU", passes, failures);
    return failures == 0 ? 0 : 1;
}
