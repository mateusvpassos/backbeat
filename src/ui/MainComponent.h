#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "engine/AudioEngine.h"
#include "engine/MidiInputRouter.h"
#include "engine/PluginHost.h"
#include "input/GlobalHotkeys.h"
#include "mapping/DrumCatalog.h"
#include "mapping/ProfileManager.h"
#include "rec/MixExporter.h"
#include "rec/Recorder.h"
#include "capture/ChromeCapture.h"
#include "ui/BeatIndicator.h"
#include "ui/ChannelStrip.h"
#include "ui/LevelMeter.h"
#include "ui/MappingWindow.h"
#include "ui/PluginWindow.h"
#include "util/Settings.h"

#include <map>
#include <memory>

namespace bb
{

class MainComponent final : public juce::Component,
                            private juce::Timer,
                            private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;

    void openMappingWindow() { toggleMappingWindow(); } // também via linha de comando: --mapeamento

private:
    // Mapeamento (F2)
    void loadCatalog (const juce::File& kitFile);
    void chooseKitFile();
    void onProfileChanged();
    void refreshProfileCombo();
    void updateGlobalHotkeys();
    void runCommand (Command c);
    void toggleMappingWindow();
    void releaseHeldKeys();

    // Gravação (F3)
    void toggleRecord();
    void togglePlay();
    void stopTransport();
    void updateTransportUi();
    void applyChromeSettings();

    // Mixer e entradas (F5)
    void setupMixer();
    void loadMixerSettings();
    void updateInputCaptions();
    void updateCompensationInfo();
    int inputCompensationSamples() const;
    void chooseRecordingsFolder();
    juce::File recordingsDirectory() const;

    // Metrônomo, contagem e exportação (F6)
    void setupMetronome();
    void loadMetronomeSettings();
    void saveMetronomeSettings();
    void tapTempo();
    void updateMetronomeUi();
    void exportMix (const juce::File& takeFolder);
    void chooseTakeToExport();
    void pollExport();
    mixexport::Gains currentMixGains() const;
    void showAbout();

    // Inicialização
    void openAudioDevice();
    void applyPreferredAsioSetup();
    void openMidiInput (const juce::String& identifier);
    void loadDrumPlugin (const juce::File& file);

    // Persistência
    void saveDeviceState();
    void savePluginState();

    // Interface
    void showAudioSettings();
    void togglePluginWindow();
    void closePluginWindow();
    void choosePluginFile();
    void showPluginMenu();                        // VST3 instalados + "Outro arquivo..." + pastas extras
    void addPluginFolder();
    juce::StringArray extraPluginFolders() const; // settings "plugin.pastas"
    void setExtraPluginFolders (const juce::StringArray& folders);
    juce::String storedStateFor (const juce::File& file) const;
    void storeStateFor (const juce::File& file, const juce::String& base64);
    void updatePluginLabels();                    // textos com o nome do plugin carregado
    void updateCatalogForPlugin();                // SSD5 = peças do .ssd5kit; outros = General MIDI
    void refreshMidiDevices();
    void refreshOutputPairs();
    void updateDeviceStatus();
    void drainMidiMonitor();
    void setStatus (const juce::String& text, bool isError = false);

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    // ---- núcleo (ordem de destruição tratada no destrutor) ----
    Settings settings { Settings::defaultDataDirectory() };
    juce::AudioDeviceManager deviceManager;
    AudioEngine engine;
    Recorder recorder { engine };
    ChromeCapture chromeCapture { engine };
    MidiInputRouter midiRouter { engine };
    PluginHost pluginHost;
    std::unique_ptr<PluginWindow> pluginWindow;
    juce::File pluginFile;
    juce::String pluginName { "sem plugin" };
    std::unique_ptr<juce::FileChooser> fileChooser;

    DrumCatalog catalog = DrumCatalog::builtInSsd5();
    ProfileManager profiles { settings.getDataDirectory() };
    MappingContext mappingContext { profiles, catalog, engine, [this] { chooseKitFile(); } };
    std::unique_ptr<MappingWindow> mappingWindow;
    GlobalHotkeys hotkeys;
    std::map<int, int> heldKeys;  // keyCode → nota tocando (teclado do PC)
    juce::StringArray hotkeyFailures;

    // ---- interface ----
    juce::Label titleLabel, subtitleLabel;
    juce::TextButton audioButton { L"Áudio..." }, openPluginButton { "Abrir plugin" },
                     choosePluginButton { "Trocar plugin" }, saveStateButton { "Salvar estado" },
                     panicButton { L"Pânico" }, refreshMidiButton { "Atualizar" };

    juce::Label deviceHeader, deviceLabel, latencyLabel, perfLabel;
    juce::Label midiHeader, midiInputLabel, timingLabel, lastNoteLabel, lastNoteDetail, lastOtherLabel;
    juce::ComboBox midiCombo, timingCombo;
    juce::Label profileLabel;
    juce::ComboBox profileCombo;
    juce::TextButton mappingButton { "Mapeamento..." };
    juce::Label transportHeader, timeLabel, transportInfoLabel, folderLabel;
    juce::TextButton recordButton { L"● Gravar" }, playButton { L"▶ Tocar" }, stopButton { L"■ Parar" },
                     chooseFolderButton { "Pasta..." }, openTakeButton { L"Abrir pasta do take" };

    juce::Label chromeHeader, chromeStatusLabel, chromeTipLabel, chromeProcessLabel;
    juce::ToggleButton chromeToggle { L"Capturar o áudio do Chrome" };
    juce::TextEditor chromeProcessEditor;
    juce::Rectangle<int> leftColumn, middleColumn, rightColumn, mixerArea, topArea;

    juce::Label metronomeHeader, bpmLabel, signatureLabel, countInLabel, metronomeVolumeLabel;
    juce::ToggleButton metronomeToggle { L"Clique" };
    juce::Slider bpmSlider, metronomeVolumeSlider;
    juce::TextButton tapButton { "Tap" }, exportButton { "Exportar mixagem" }, exportOtherButton { "..." },
                     aboutButton { "Sobre" };
    juce::ComboBox signatureCombo, countInCombo;
    BeatIndicator beatIndicator;
    std::vector<double> tapTimes;

    class ExportJob;
    std::unique_ptr<ExportJob> exportJob;

    juce::Label mixerHeader, outputPairLabel, compensationLabel, compensationInfo, mixerHint;
    juce::ComboBox outputCombo;
    juce::Slider compensationSlider;
    std::unique_ptr<ChannelStrip> strips[numTracks];
    std::unique_ptr<ChannelStrip> masterStrip;
    std::unique_ptr<juce::Drawable> logo;
    juce::Label statusLabel;

    juce::Array<juce::MidiDeviceInfo> midiDevices;
    juce::Array<int> outputPairStarts;
    int timerTicks = 0;
    float noteFlash = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace bb
