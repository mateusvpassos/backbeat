#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "engine/AudioEngine.h"
#include "engine/MidiInputRouter.h"
#include "engine/PluginHost.h"
#include "input/GlobalHotkeys.h"
#include "mapping/DrumCatalog.h"
#include "mapping/ProfileManager.h"
#include "rec/Recorder.h"
#include "capture/ChromeCapture.h"
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
    void chooseRecordingsFolder();
    juce::File recordingsDirectory() const;

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
    juce::TextButton audioButton { L"Áudio..." }, openPluginButton { "Abrir SSD5" },
                     choosePluginButton { "Trocar plugin..." }, saveStateButton { "Salvar estado" },
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

    juce::Label chromeHeader, chromeStatusLabel, chromeGainLabel, chromeTipLabel, chromeProcessLabel;
    juce::ToggleButton chromeToggle { L"Capturar o áudio do Chrome" }, chromeMuteButton { "Mudo" };
    juce::TextEditor chromeProcessEditor;
    juce::Slider chromeGainSlider;
    LevelMeter chromeMeterLeft, chromeMeterRight;
    juce::Rectangle<int> leftColumn, rightColumn;
    juce::Label outputHeader, outputPairLabel, gainLabel;
    juce::ComboBox outputCombo;
    juce::Slider gainSlider;
    LevelMeter meterLeft, meterRight;
    juce::Label statusLabel;

    juce::Array<juce::MidiDeviceInfo> midiDevices;
    juce::Array<int> outputPairStarts;
    int timerTicks = 0;
    float noteFlash = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace bb
