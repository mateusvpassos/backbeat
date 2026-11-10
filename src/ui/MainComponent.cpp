#include "ui/MainComponent.h"

#include "ui/AboutWindow.h"
#include "util/DrumNames.h"

#include "BackbeatAssets.h"

namespace bb
{

namespace
{
    // Cores e estilos da família (ui/Theme.h).
    const juce::Colour& background = theme::bg;
    const juce::Colour& panel = theme::panel;
    const juce::Colour& accent = theme::accent;
    const juce::Colour& textDim = theme::muted;
    const juce::Colour& errorColour = theme::danger;

    const char* const trackKeys[numTracks] = { "bateria", "chrome", "entrada1", "entrada2" };

    float dbToGain (double db) { return db <= -59.95 ? 0.0f : juce::Decibels::decibelsToGain ((float) db); }

    juce::String ms (double samples, double sampleRate)
    {
        if (sampleRate <= 0.0)
            return "?";
        return juce::String (samples * 1000.0 / sampleRate, 1).replaceCharacter ('.', ',') + " ms";
    }

    void styleHeader (juce::Label& l, const juce::String& text) { theme::header (l, text); }

    void styleBody (juce::Label& l)
    {
        l.setFont (theme::font (13.5f));
        l.setColour (juce::Label::textColourId, theme::text);
    }

    void styleCaption (juce::Label& l, const juce::String& text) { theme::caption (l, text); }
}

// Exporta a mixagem numa thread própria (pode levar alguns segundos num take longo).
class MainComponent::ExportJob final : public juce::Thread
{
public:
    ExportJob (juce::File takeFolder, const mixexport::Gains& g)
        : juce::Thread ("Backbeat: exportar mixagem"), folder (std::move (takeFolder)), gains (g)
    {
        startThread();
    }

    ~ExportJob() override { stopThread (10000); } // fechar no meio: cancela e apaga o temporário

    void run() override
    {
        result = mixexport::exportTake (folder, gains, [this] (float p)
        {
            progress.store (p);
            return ! threadShouldExit();
        });
        done.store (true);
    }

    const juce::File folder;
    const mixexport::Gains gains;
    mixexport::Result result;          // lido só depois de done
    std::atomic<float> progress { 0.0f };
    std::atomic<bool> done { false };
};

MainComponent::MainComponent()
{
    // ---------- interface ----------
    titleLabel.setText ("Backbeat", juce::dontSendNotification);
    titleLabel.setFont (theme::font (16.0f, theme::Weight::semibold));
    titleLabel.setColour (juce::Label::textColourId, theme::text);
    subtitleLabel.setText (L"bateria eletrônica + SSD5, via ASIO", juce::dontSendNotification);
    subtitleLabel.setFont (theme::font (12.5f));
    subtitleLabel.setColour (juce::Label::textColourId, textDim);
    logo = juce::Drawable::createFromImageData (BackbeatAssets::icon_svg, BackbeatAssets::icon_svgSize);

    styleHeader (deviceHeader, L"Áudio");
    styleBody (deviceLabel);
    styleBody (latencyLabel);
    styleBody (perfLabel);
    perfLabel.setColour (juce::Label::textColourId, textDim);

    styleHeader (midiHeader, "Bateria (MIDI)");
    styleCaption (midiInputLabel, "Entrada");
    styleCaption (timingLabel, "Tempo das notas");
    styleCaption (profileLabel, "Perfil");

    styleHeader (transportHeader, L"Gravação");
    timeLabel.setFont (theme::mono (26.0f));
    timeLabel.setColour (juce::Label::textColourId, theme::text);
    styleBody (transportInfoLabel);
    styleCaption (folderLabel, {});
    recordButton.setTooltip (L"Grava o MIDI e o áudio da bateria (F9). Dica: o modo Ritmo estável deixa o tempo das notas exato.");
    playButton.setTooltip (L"Toca o último take pelo SSD5, com o kit atual (F10)");
    stopButton.setTooltip (L"Para a gravação ou a reprodução (F8)");
    chooseFolderButton.setTooltip (L"Onde as gravações são salvas");

    styleHeader (chromeHeader, "Chrome (Songsterr)");
    styleBody (chromeStatusLabel);
    styleCaption (chromeProcessLabel, "Processo");
    styleCaption (chromeTipLabel, L"Para não ouvir o Chrome em dobro: Configurações › Sistema › Som › Mixer de volume › "
                                  L"Chrome › Dispositivo de saída = uma saída que você não usa (ex.: alto-falantes da placa-mãe). "
                                  L"A captura pega o áudio do Chrome em qualquer saída e o Backbeat toca pela Behringer.");
    chromeTipLabel.setJustificationType (juce::Justification::topLeft);
    chromeTipLabel.setMinimumHorizontalScale (1.0f);
    chromeProcessEditor.setTooltip (L"Nome do executável (ex.: chrome.exe, msedge.exe, spotify.exe)");
    mappingButton.setTooltip (L"Escolher qual peça do SSD5 cada pad e cada tecla toca");
    lastNoteLabel.setFont (theme::font (26.0f, theme::Weight::semibold));
    lastNoteLabel.setText ("bata num pad", juce::dontSendNotification);
    lastNoteLabel.setColour (juce::Label::textColourId, textDim);
    styleBody (lastNoteDetail);
    styleCaption (lastOtherLabel, {});

    styleHeader (mixerHeader, "Mixer");
    styleCaption (outputPairLabel, L"Saída");
    styleCaption (compensationLabel, L"Ajuste fino das entradas");
    styleCaption (compensationInfo, {});
    compensationInfo.setFont (theme::mono (11.5f));
    styleCaption (mixerHint, L"A gravação é sempre antes do fader: volume, mudo e solo só mudam o que você ouve.");
    mixerHint.setFont (theme::font (11.5f));
    mixerHint.setColour (juce::Label::textColourId, theme::dim);

    styleCaption (statusLabel, {});

    styleHeader (metronomeHeader, L"Metrônomo");
    styleCaption (bpmLabel, "BPM");
    styleCaption (signatureLabel, "Compasso");
    styleCaption (countInLabel, "Contagem");
    styleCaption (metronomeVolumeLabel, "Volume");
    exportButton.setTooltip (L"Junta as faixas do último take em mixagem.wav, com os volumes, mudo e solo atuais do mixer");
    exportOtherButton.setTooltip (L"Exportar a mixagem de outro take...");
    aboutButton.getProperties().set ("variant", "ghost");

    timingCombo.addItem (L"Menor latência", 1 + (int) AudioEngine::MidiTiming::lowestLatency);
    timingCombo.addItem (L"Ritmo estável (+1 buffer)", 1 + (int) AudioEngine::MidiTiming::steady);


    for (auto* c : std::initializer_list<juce::Component*> {
             &titleLabel, &subtitleLabel, &audioButton, &openPluginButton, &choosePluginButton,
             &saveStateButton, &panicButton, &deviceHeader, &deviceLabel, &latencyLabel, &perfLabel,
             &midiHeader, &midiInputLabel, &midiCombo, &refreshMidiButton, &timingLabel, &timingCombo,
             &profileLabel, &profileCombo, &mappingButton,
             &transportHeader, &recordButton, &playButton, &stopButton, &timeLabel, &transportInfoLabel,
             &folderLabel, &chooseFolderButton, &openTakeButton,
             &chromeHeader, &chromeToggle, &chromeProcessLabel, &chromeProcessEditor, &chromeStatusLabel,
             &chromeTipLabel, &lastNoteLabel, &lastNoteDetail, &lastOtherLabel,
             &mixerHeader, &outputPairLabel, &outputCombo, &compensationLabel, &compensationSlider,
             &compensationInfo, &mixerHint, &statusLabel,
             &metronomeHeader, &beatIndicator, &metronomeToggle, &bpmLabel, &bpmSlider, &tapButton,
             &signatureLabel, &signatureCombo, &countInLabel, &countInCombo, &metronomeVolumeLabel,
             &metronomeVolumeSlider, &exportButton, &exportOtherButton, &aboutButton })
        addAndMakeVisible (c);

    openPluginButton.getProperties().set ("variant", "primary");
    recordButton.getProperties().set ("onColour", (juce::int64) theme::danger.getARGB());
    playButton.getProperties().set ("onColour", (juce::int64) theme::ok.getARGB());
    setupMixer();
    setupMetronome();

    exportButton.onClick       = [this] { if (const auto* take = recorder.getLastTake()) exportMix (take->folder); };
    exportOtherButton.onClick  = [this] { chooseTakeToExport(); };
    aboutButton.onClick        = [this] { showAbout(); };
    audioButton.onClick        = [this] { showAudioSettings(); };
    openPluginButton.onClick   = [this] { togglePluginWindow(); };
    choosePluginButton.onClick = [this] { choosePluginFile(); };
    saveStateButton.onClick    = [this] { savePluginState(); setStatus ("Estado do plugin salvo."); };
    panicButton.onClick        = [this] { engine.panic(); setStatus ("Todas as notas paradas."); };
    refreshMidiButton.onClick  = [this] { refreshMidiDevices(); };
    mappingButton.onClick      = [this] { toggleMappingWindow(); };
    recordButton.onClick       = [this] { toggleRecord(); };
    playButton.onClick         = [this] { togglePlay(); };
    stopButton.onClick         = [this] { stopTransport(); };
    chooseFolderButton.onClick = [this] { chooseRecordingsFolder(); };
    openTakeButton.onClick     = [this]
    {
        if (const auto* take = recorder.getLastTake())
            take->folder.startAsProcess();
        else
            recordingsDirectory().startAsProcess();
    };

    profileCombo.onChange = [this]
    {
        const auto name = profileCombo.getText();
        if (name.isNotEmpty() && name != profiles.current().name)
            profiles.select (name);
    };

    midiCombo.onChange = [this]
    {
        const int index = midiCombo.getSelectedItemIndex();
        if (juce::isPositiveAndBelow (index, midiDevices.size()))
            openMidiInput (midiDevices[index].identifier);
    };

    timingCombo.onChange = [this]
    {
        const int mode = timingCombo.getSelectedId() - 1;
        engine.setMidiTiming ((AudioEngine::MidiTiming) juce::jlimit (0, 1, mode));
        settings.set ("midi", "tempo", mode == 1 ? "estavel" : "rapido");
        settings.save();
        updateDeviceStatus();
    };

    outputCombo.onChange = [this]
    {
        const int index = outputCombo.getSelectedItemIndex();
        if (juce::isPositiveAndBelow (index, outputPairStarts.size()))
        {
            engine.setOutputPair (outputPairStarts[index]);
            settings.set ("saida", "par", outputPairStarts[index]);
            settings.save();
        }
    };

    setSize (1440, 770);

    // ---------- núcleo ----------
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    const auto step = [t0] (const char* what)
    {
        juce::Logger::writeToLog (juce::String (juce::roundToInt (juce::Time::getMillisecondCounterHiRes() - t0))
                                  + " ms  " + what);
    };

    settings.load();
    step ("configuracoes lidas");

    engine.setMidiTiming (settings.get ("midi", "tempo", "rapido").toString() == "estavel"
                              ? AudioEngine::MidiTiming::steady : AudioEngine::MidiTiming::lowestLatency);
    timingCombo.setSelectedId (1 + (int) engine.getMidiTiming(), juce::dontSendNotification);

    engine.setOutputPair ((int) settings.get ("saida", "par", 0));
    loadMixerSettings();
    loadMetronomeSettings();

    openAudioDevice();
    step ("dispositivo de audio aberto");
    deviceManager.addAudioCallback (&engine);
    deviceManager.addChangeListener (this);
    refreshOutputPairs();
    updateInputCaptions();

    refreshMidiDevices();
    step ("entrada MIDI aberta");

    // Mapeamento: peças do kit do SSD5, perfil ativo e atalhos globais.
    loadCatalog (juce::File (settings.get ("ssd5", "kit", DrumCatalog::defaultSsd5KitFile().getFullPathName()).toString()));
    step ("catalogo de pecas lido");

    hotkeys.onHotkey = [this] (int index)
    {
        const auto& keys = profiles.current().keys;
        if (juce::isPositiveAndBelow (index, keys.size()))
            runCommand (keys.getReference (index).command);
    };
    hotkeys.onRegistrationResult = [this] (const juce::StringArray& failed)
    {
        hotkeyFailures = failed;
        juce::Logger::writeToLog ("atalhos globais registrados"
                                  + (failed.isEmpty() ? juce::String() : ", falharam: " + failed.joinIntoString (", ")));
        if (! failed.isEmpty())
            setStatus (L"Atalho global não registrado: " + failed.joinIntoString (", ") + " (outro programa usa?)", true);
        if (mappingWindow != nullptr)
            mappingWindow->setHotkeyFailures (failed);
    };

    profiles.onChanged = [this] { onProfileChanged(); };
    profiles.loadOrCreate (settings.get ("midi", "perfil").toString());
    step ("perfil carregado");

    // Gravação: último take (para o Tocar) e avisos do gravador.
    recorder.onMessage = [this] (const juce::String& text, bool isError)
    {
        juce::Logger::writeToLog ((isError ? "ERRO gravacao: " : "gravacao: ") + text);
        setStatus (text, isError);
    };
    recorder.onStateChanged = [this] { updateTransportUi(); };
    recorder.loadLatestTake (recordingsDirectory());

    // Chrome (F4)
    chromeProcessEditor.setText (settings.get ("chrome", "processo", "chrome.exe").toString(), false);
    chromeToggle.setToggleState ((bool) settings.get ("chrome", "ativo", false), juce::dontSendNotification);

    chromeToggle.onClick = [this] { applyChromeSettings(); };
    chromeProcessEditor.onReturnKey = [this] { applyChromeSettings(); };
    chromeProcessEditor.onFocusLost = [this] { if (chromeProcessEditor.getText() != chromeCapture.getExeName()) applyChromeSettings(); };
    applyChromeSettings();
    step ("captura do Chrome configurada");
    updateTransportUi();
    step ("ultimo take lido");

    setWantsKeyboardFocus (true);

    // Carrega o plugin depois que a janela aparece (o SSD5 leva alguns segundos).
    pluginFile = juce::File (settings.get ("plugin", "caminho",
                                           PluginHost::defaultDrumPluginFile().getFullPathName()).toString());
    setStatus ("Carregando " + pluginFile.getFileName() + "...");
    juce::Component::SafePointer<MainComponent> safe (this);
    juce::Timer::callAfterDelay (200, [safe]
    {
        if (safe != nullptr)
            safe->loadDrumPlugin (safe->pluginFile);
    });

    updateDeviceStatus();
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    exportJob.reset(); // cancela uma exportação em andamento
    deviceManager.removeChangeListener (this);

    releaseHeldKeys();
    profiles.onChanged = nullptr;
    hotkeys.onHotkey = nullptr;
    hotkeys.onRegistrationResult = nullptr;
    mappingWindow.reset();

    savePluginState();
    saveDeviceState();

    recorder.onMessage = nullptr;
    recorder.onStateChanged = nullptr;
    chromeProcessEditor.onFocusLost = nullptr;
    recorder.stop();                           // 0. fechar com gravação em andamento: salva o take

    closePluginWindow();                       // 1. editor antes do plugin
    midiRouter.close();                        // 2. para de receber MIDI
    deviceManager.removeAudioCallback (&engine);
    deviceManager.closeAudioDevice();          // 3. para o ASIO
    recorder.poll();                           //    (áudio parado: finaliza WAV, .mid e sessao.json)
    engine.setPlugin (nullptr);                // 4. destrói o plugin
}

// ---------------------------------------------------------------------------
// Áudio
// ---------------------------------------------------------------------------

void MainComponent::openAudioDevice()
{
    std::unique_ptr<juce::XmlElement> saved;
    const auto xmlText = settings.get ("audio", "dispositivo").toString();
    if (xmlText.isNotEmpty())
        saved = juce::parseXML (xmlText);

    // 2 entradas já reservadas para a F5 (mic/guitarra da Behringer); 2 saídas.
    const auto error = deviceManager.initialise (2, 2, saved.get(), saved == nullptr);

    auto* device = deviceManager.getCurrentAudioDevice();
    const bool isAsio = device != nullptr && device->getTypeName() == "ASIO";

    if (saved == nullptr || error.isNotEmpty() || ! isAsio)
        applyPreferredAsioSetup();
}

void MainComponent::applyPreferredAsioSetup()
{
    deviceManager.setCurrentAudioDeviceType ("ASIO", true);

    auto* type = deviceManager.getCurrentDeviceTypeObject();
    if (type == nullptr || type->getTypeName() != "ASIO")
    {
        setStatus (L"ASIO não disponível. Escolha um dispositivo em Áudio...", true);
        return;
    }

    type->scanForDevices();
    const auto names = type->getDeviceNames (false);

    juce::String chosen;
    for (const auto& n : names)
        if (n.containsIgnoreCase ("BEHRINGER"))
            chosen = n;

    if (chosen.isEmpty() && ! names.isEmpty())
        chosen = names[0];

    if (chosen.isEmpty())
    {
        setStatus ("Nenhum driver ASIO encontrado.", true);
        return;
    }

    auto setup = deviceManager.getAudioDeviceSetup();
    setup.outputDeviceName = chosen;
    setup.inputDeviceName = chosen;
    setup.sampleRate = 44100.0;
    setup.bufferSize = 256;
    setup.useDefaultInputChannels = true;
    setup.useDefaultOutputChannels = true;

    const auto error = deviceManager.setAudioDeviceSetup (setup, true);
    if (error.isNotEmpty())
        setStatus (L"Não abriu " + chosen + ": " + error
                   + L" (o Voicemeeter ou outro programa está usando o ASIO?)", true);
    else
        saveDeviceState();
}

void MainComponent::saveDeviceState()
{
    if (auto xml = deviceManager.createStateXml())
    {
        settings.set ("audio", "dispositivo", xml->toString (juce::XmlElement::TextFormat().singleLine()));
        settings.save();
    }
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (deviceManager,
                                                             0, 8,   // entradas (min, max)
                                                             2, 8,   // saídas
                                                             false, false, false, false);
    selector->setSize (560, 420);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (selector);
    options.dialogTitle = L"Configurações de áudio";
    options.dialogBackgroundColour = panel;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Dispositivo mudou (diálogo de áudio, driver resetou, etc.).
    saveDeviceState();
    refreshOutputPairs();
    updateInputCaptions();
    updateDeviceStatus();
}

void MainComponent::refreshOutputPairs()
{
    outputCombo.clear (juce::dontSendNotification);
    outputPairStarts.clear();

    auto* device = deviceManager.getCurrentAudioDevice();
    if (device == nullptr)
        return;

    // O callback recebe só os canais ativos, em sequência: o índice do par é a posição entre os ativos.
    const auto names = device->getOutputChannelNames();
    const auto active = device->getActiveOutputChannels();

    juce::StringArray activeNames;
    for (int i = 0; i < names.size(); ++i)
        if (active[i])
            activeNames.add (names[i]);

    for (int k = 0; k + 1 < activeNames.size(); k += 2)
    {
        outputPairStarts.add (k);
        outputCombo.addItem (activeNames[k] + " + " + activeNames[k + 1], k + 1);
    }

    if (activeNames.size() == 1)
    {
        outputPairStarts.add (0);
        outputCombo.addItem (activeNames[0], 1);
    }

    const int wanted = engine.getOutputPair();
    const int index = juce::jmax (0, outputPairStarts.indexOf (wanted));
    outputCombo.setSelectedItemIndex (index, juce::dontSendNotification);
    if (! outputPairStarts.isEmpty())
        engine.setOutputPair (outputPairStarts[index]);
}

void MainComponent::updateDeviceStatus()
{
    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
    {
        deviceLabel.setText (L"Nenhum dispositivo de áudio aberto", juce::dontSendNotification);
        deviceLabel.setColour (juce::Label::textColourId, errorColour);
        latencyLabel.setText ({}, juce::dontSendNotification);
        perfLabel.setText ({}, juce::dontSendNotification);
        return;
    }

    const double sr = device->getCurrentSampleRate();
    const int bs = device->getCurrentBufferSizeSamples();
    const int inLat = device->getInputLatencyInSamples();
    const int outLat = device->getOutputLatencyInSamples();
    const int pluginLat = engine.getPlugin() != nullptr ? engine.getPlugin()->getLatencySamples() : 0;
    const bool steady = engine.getMidiTiming() == AudioEngine::MidiTiming::steady;

    deviceLabel.setColour (juce::Label::textColourId, theme::text);
    updateCompensationInfo();
    deviceLabel.setText (device->getTypeName() + L" · " + device->getName() + L" · "
                             + juce::String ((int) sr) + L" Hz · buffer " + juce::String (bs)
                             + " (" + ms (bs, sr) + ")",
                         juce::dontSendNotification);

    // Pad → som: o MIDI espera o próximo bloco (até 1 buffer, ou 1 buffer fixo no modo estável),
    // o plugin processa, e o driver leva a latência de saída até o conversor.
    const double padToSoundMin = (steady ? bs : 0) + outLat + pluginLat;
    const double padToSoundMax = bs + outLat + pluginLat;

    juce::String latency = L"Latência  entrada " + ms (inLat, sr) + L" · saída " + ms (outLat, sr);
    if (pluginLat > 0)
        latency << L" · plugin " << ms (pluginLat, sr);
    latency << L"   |   pad → som: ";
    latency << (steady ? ms (padToSoundMax, sr)
                       : ms (padToSoundMin, sr) + " a " + ms (padToSoundMax, sr));
    latencyLabel.setText (latency, juce::dontSendNotification);

    const int xruns = device->getXRunCount();
    juce::String perf = L"CPU do áudio " + juce::String (juce::roundToInt (deviceManager.getCpuUsage() * 100.0)) + "%";
    perf << L" · falhas (xruns) " << (xruns >= 0 ? juce::String (xruns) : juce::String ("n/d"));
    if (const auto dropped = engine.getDroppedMidiCount())
        perf << L" · MIDI perdido " << (int) dropped;
    perfLabel.setText (perf, juce::dontSendNotification);
}

// ---------------------------------------------------------------------------
// MIDI
// ---------------------------------------------------------------------------

void MainComponent::refreshMidiDevices()
{
    midiDevices = MidiInputRouter::availableDevices();
    midiCombo.clear (juce::dontSendNotification);

    for (int i = 0; i < midiDevices.size(); ++i)
        midiCombo.addItem (midiDevices[i].name, i + 1);

    if (midiDevices.isEmpty())
    {
        midiCombo.setTextWhenNoChoicesAvailable (L"Nenhuma entrada MIDI (a EDRUM está ligada?)");
        midiCombo.setTextWhenNothingSelected (L"Nenhuma entrada MIDI: ligue a EDRUM e clique em Atualizar");
        midiRouter.close();
        return;
    }

    const auto current = midiRouter.isOpen() ? midiRouter.getOpenIdentifier()
                                             : settings.get ("midi", "entrada").toString();
    const auto preferred = MidiInputRouter::findPreferredDevice (current);

    for (int i = 0; i < midiDevices.size(); ++i)
        if (midiDevices[i].identifier == preferred)
            midiCombo.setSelectedItemIndex (i, juce::dontSendNotification);

    if (! midiRouter.isOpen() || midiRouter.getOpenIdentifier() != preferred)
        openMidiInput (preferred);
}

void MainComponent::openMidiInput (const juce::String& identifier)
{
    if (midiRouter.open (identifier))
    {
        settings.set ("midi", "entrada", identifier);
        settings.set ("midi", "entradaNome", midiRouter.getOpenName());
        settings.save();
        setStatus ("MIDI: " + midiRouter.getOpenName() + " aberto.");
    }
    else
    {
        setStatus (L"Não foi possível abrir a entrada MIDI (outro programa usando?)", true);
    }
}

void MainComponent::drainMidiMonitor()
{
    MidiEvent e;
    while (midiRouter.popMonitorEvent (e))
    {
        const auto m = juce::MidiMessage (e.data, (int) e.size);

        if (mappingWindow != nullptr)
            mappingWindow->handleMidi (e);

        if (m.isNoteOn())
        {
            lastNoteLabel.setText (describeDrumNote (m.getNoteNumber()), juce::dontSendNotification);
            lastNoteDetail.setText ((e.mapped >= 0 ? L"→ SSD5: " + catalog.nameFor (e.mapped) + " (" + juce::String (e.mapped) + ")"
                                                   : juce::String (L"→ ignorado pelo perfil"))
                                        + L"  ·  força " + juce::String (m.getVelocity())
                                        + L" · canal " + juce::String (m.getChannel()),
                                    juce::dontSendNotification);
            noteFlash = 1.0f;
        }
        else if (m.isController())
        {
            // Ex.: CC4 = abertura do chimbal (pedal) na Nexus.
            lastOtherLabel.setText ("CC" + juce::String (m.getControllerNumber()) + " = "
                                        + juce::String (m.getControllerValue())
                                        + "  (canal " + juce::String (m.getChannel()) + ")",
                                    juce::dontSendNotification);
        }
        else if (m.isAftertouch() || m.isChannelPressure())
        {
            lastOtherLabel.setText ("Abafamento (aftertouch) nota " + juce::String (m.getNoteNumber()),
                                    juce::dontSendNotification);
        }
    }

    noteFlash = juce::jmax (0.0f, noteFlash - 0.06f);
    lastNoteLabel.setColour (juce::Label::textColourId,
                             textDim.interpolatedWith (accent, noteFlash).withAlpha (1.0f));
}

// ---------------------------------------------------------------------------
// Plugin
// ---------------------------------------------------------------------------

void MainComponent::loadDrumPlugin (const juce::File& file)
{
    savePluginState();
    closePluginWindow();

    const double sr = engine.getSampleRate() > 0.0 ? engine.getSampleRate() : 44100.0;
    const int bs = engine.getBlockSize() > 0 ? engine.getBlockSize() : 256;

    const auto loadStart = juce::Time::getMillisecondCounterHiRes();
    auto result = pluginHost.loadVst3 (file, sr, bs);
    juce::Logger::writeToLog ("plugin " + file.getFileName() + ": "
                              + (result.instance != nullptr ? juce::String ("ok") : "erro: " + result.error)
                              + " em " + juce::String (juce::roundToInt (juce::Time::getMillisecondCounterHiRes() - loadStart)) + " ms");
    if (result.instance == nullptr)
    {
        setStatus (result.error, true);
        openPluginButton.setEnabled (false);
        saveStateButton.setEnabled (false);
        return;
    }

    // Restaura kit e mixer interno se o estado salvo for deste mesmo plugin.
    bool restored = false;
    if (settings.get ("plugin", "estadoDe").toString() == file.getFullPathName())
        restored = PluginHost::setStateBase64 (*result.instance, settings.get ("plugin", "estado").toString());

    const auto name = result.instance->getName();
    engine.setPlugin (std::move (result.instance)); // o anterior (se houver) é destruído aqui, fora do áudio

    pluginFile = file;
    settings.set ("plugin", "caminho", file.getFullPathName());
    settings.save();

    openPluginButton.setButtonText ("Abrir " + name);
    openPluginButton.setEnabled (true);
    saveStateButton.setEnabled (true);
    setStatus (name + " carregado" + (restored ? ", estado restaurado." : ". Escolha o kit e clique em Salvar estado."));
    updateDeviceStatus();
}

void MainComponent::savePluginState()
{
    if (auto* p = engine.getPlugin())
    {
        settings.set ("plugin", "estado", PluginHost::getStateBase64 (*p));
        settings.set ("plugin", "estadoDe", pluginFile.getFullPathName());
        settings.save();
    }
}

void MainComponent::togglePluginWindow()
{
    if (pluginWindow != nullptr)
    {
        pluginWindow->toFront (true);
        return;
    }

    auto* p = engine.getPlugin();
    if (p == nullptr)
        return;

    pluginWindow = std::make_unique<PluginWindow> (*p);

    juce::Component::SafePointer<MainComponent> safe (this);
    pluginWindow->onClose = [safe]
    {
        // Fechar a janela do plugin salva o estado (kit escolhido) automaticamente.
        juce::MessageManager::callAsync ([safe]
        {
            if (safe != nullptr)
            {
                safe->savePluginState();
                safe->closePluginWindow();
                safe->setStatus ("Estado do plugin salvo.");
            }
        });
    };
}

void MainComponent::closePluginWindow()
{
    pluginWindow.reset();
}

void MainComponent::choosePluginFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Escolha um plugin VST3",
                                                       PluginHost::defaultDrumPluginFile().getParentDirectory(),
                                                       "*.vst3");

    // .vst3 é uma pasta (bundle): permite escolher pasta ou arquivo.
    const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::canSelectDirectories;

    juce::Component::SafePointer<MainComponent> safe (this);
    fileChooser->launchAsync (chooserFlags, [safe] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (safe != nullptr && file != juce::File())
            safe->loadDrumPlugin (file);
    });
}

// ---------------------------------------------------------------------------
// Timer, status e layout
// ---------------------------------------------------------------------------

void MainComponent::timerCallback()
{
    drainMidiMonitor();
    for (int t = 0; t < numTracks; ++t)
    {
        const float peak = engine.takeTrackPeak (t);
        strips[t]->setLevels (peak, peak, engine.takeTrackClip (t));
    }
    masterStrip->setLevels (engine.takeMasterPeakLeft(), engine.takeMasterPeakRight(), engine.takeMasterClip());

    if (timerTicks % 15 == 7) // 2x por segundo, defasado do status do dispositivo
    {
        chromeCapture.poll();
        chromeStatusLabel.setText (chromeCapture.getStatusText(), juce::dontSendNotification);
        chromeStatusLabel.setColour (juce::Label::textColourId, chromeCapture.hasProblem() ? theme::warn : theme::text);
    }

    recorder.poll();
    if (recorder.getState() != Recorder::State::idle || timerTicks % 10 == 0)
        updateTransportUi();
    updateMetronomeUi();
    pollExport();

    if (++timerTicks % 15 == 0) // 2x por segundo
        updateDeviceStatus();
}

void MainComponent::setStatus (const juce::String& text, bool isError)
{
    statusLabel.setText (text, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, isError ? errorColour : textDim);
    if (isError)
        juce::Logger::writeToLog ("ERRO: " + text);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (background);

    // Marca (como a barra de título dos outros apps): ícone 26px + nome.
    if (logo != nullptr)
        logo->drawWithin (g, juce::Rectangle<float> (20.0f, 16.0f, 30.0f, 30.0f), juce::RectanglePlacement::centred, 1.0f);

    // Painéis da família atrás de cada seção, na largura da coluna.
    const auto drawPanel = [&] (juce::Component& first, juce::Component& last, juce::Rectangle<int> column)
    {
        auto r = first.getBounds().getUnion (last.getBounds()).expanded (14, 12).toFloat();
        r.setLeft ((float) column.getX() - 14.0f);
        r.setRight ((float) column.getRight() + 14.0f);
        theme::drawPanel (g, r);
    };

    // O último painel de cada coluna desce até o fim da área de cima: as três colunas terminam alinhadas.
    const auto drawLastPanel = [&] (juce::Component& first, juce::Rectangle<int> column)
    {
        auto r = juce::Rectangle<int>::leftTopRightBottom (column.getX(), first.getY(), column.getRight(), topArea.getBottom())
                     .expanded (14, 12).toFloat();
        theme::drawPanel (g, r);
    };

    drawPanel (deviceHeader, perfLabel, leftColumn);
    drawLastPanel (midiHeader, leftColumn);
    drawPanel (transportHeader, folderLabel, middleColumn);
    drawLastPanel (metronomeHeader, middleColumn);
    drawLastPanel (chromeHeader, rightColumn);
    theme::drawPanel (g, mixerArea.expanded (14, 0).toFloat()); // mesmas bordas dos painéis de cima

    // linha da barra de título
    g.setColour (theme::border);
    g.fillRect (0, 60, getWidth(), 1);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    statusLabel.setBounds (area.removeFromBottom (28).reduced (24, 4));

    // ---- barra de título: marca à esquerda, ações à direita ----
    {
        auto top = area.removeFromTop (60).reduced (20, 15);
        top.removeFromLeft (40); // ícone
        titleLabel.setBounds (top.removeFromLeft (90));
        subtitleLabel.setBounds (top.removeFromLeft (260));

        for (auto* b : { &aboutButton, &panicButton, &saveStateButton, &choosePluginButton, &openPluginButton, &audioButton })
        {
            b->setBounds (top.removeFromRight (b == &openPluginButton ? 160 : b == &aboutButton ? 80 : 118));
            top.removeFromRight (8);
        }
    }

    area = area.reduced (24, 0);
    area.removeFromTop (22);

    // ---- mixer embaixo, largura toda ----
    mixerArea = area.removeFromBottom (318).withTrimmedBottom (6);
    {
        auto m = mixerArea.reduced (0, 12);
        auto head = m.removeFromTop (18);
        mixerHeader.setBounds (head.removeFromLeft (80));
        mixerHint.setBounds (head);
        m.removeFromTop (10);

        constexpr int stripW = 170, gap = 10;
        for (int t = 0; t < numTracks; ++t)
        {
            strips[t]->setBounds (m.removeFromLeft (stripW));
            m.removeFromLeft (gap);
        }
        masterStrip->setBounds (m.removeFromLeft (stripW));
        m.removeFromLeft (gap + 14);

        // saída + compensação das entradas
        auto side = m.removeFromLeft (juce::jmin (m.getWidth(), 380));
        outputPairLabel.setBounds (side.removeFromTop (20));
        outputCombo.setBounds (side.removeFromTop (28));
        side.removeFromTop (18);
        compensationLabel.setBounds (side.removeFromTop (20));
        compensationSlider.setBounds (side.removeFromTop (28));
        side.removeFromTop (4);
        compensationInfo.setBounds (side.removeFromTop (54));
    }
    area.removeFromBottom (24);
    topArea = area;

    const int gap = 40;
    const int colW = (area.getWidth() - 2 * gap) / 3;
    leftColumn = area.removeFromLeft (colW);
    area.removeFromLeft (gap);
    middleColumn = area.removeFromLeft (colW);
    area.removeFromLeft (gap);
    rightColumn = area;

    // ================= coluna esquerda =================
    auto col = leftColumn;

    deviceHeader.setBounds (col.removeFromTop (18));
    col.removeFromTop (4);
    deviceLabel.setBounds (col.removeFromTop (22));
    latencyLabel.setBounds (col.removeFromTop (22));
    perfLabel.setBounds (col.removeFromTop (20));
    col.removeFromTop (34);

    midiHeader.setBounds (col.removeFromTop (18));
    col.removeFromTop (6);
    {
        auto row = col.removeFromTop (28);
        midiInputLabel.setBounds (row.removeFromLeft (110));
        refreshMidiButton.setBounds (row.removeFromRight (90));
        row.removeFromRight (8);
        midiCombo.setBounds (row);
    }
    col.removeFromTop (6);
    {
        auto row = col.removeFromTop (28);
        timingLabel.setBounds (row.removeFromLeft (110));
        timingCombo.setBounds (row.removeFromLeft (230));
    }
    col.removeFromTop (6);
    {
        auto row = col.removeFromTop (28);
        profileLabel.setBounds (row.removeFromLeft (110));
        mappingButton.setBounds (row.removeFromRight (130));
        row.removeFromRight (8);
        profileCombo.setBounds (row);
    }
    col.removeFromTop (8);
    lastNoteLabel.setBounds (col.removeFromTop (36));
    lastNoteDetail.setBounds (col.removeFromTop (20));
    lastOtherLabel.setBounds (col.removeFromTop (18));

    // ================= coluna do meio: gravação + metrônomo =================
    col = middleColumn;

    transportHeader.setBounds (col.removeFromTop (18));
    col.removeFromTop (6);
    {
        auto row = col.removeFromTop (34);
        for (auto* b : { &recordButton, &playButton, &stopButton })
        {
            b->setBounds (row.removeFromLeft (94));
            row.removeFromLeft (6);
        }
        row.removeFromLeft (6);
        timeLabel.setBounds (row);
    }
    col.removeFromTop (6);
    transportInfoLabel.setBounds (col.removeFromTop (22));
    col.removeFromTop (6);
    {
        auto row = col.removeFromTop (28);
        exportButton.setBounds (row.removeFromLeft (150));
        row.removeFromLeft (4);
        exportOtherButton.setBounds (row.removeFromLeft (34));
        openTakeButton.setBounds (row.removeFromRight (150));
    }
    col.removeFromTop (6);
    {
        auto row = col.removeFromTop (28);
        chooseFolderButton.setBounds (row.removeFromRight (80));
        row.removeFromRight (8);
        folderLabel.setBounds (row);
    }
    col.removeFromTop (34);

    {
        auto head = col.removeFromTop (18);
        metronomeHeader.setBounds (head.removeFromLeft (140));
        beatIndicator.setBounds (head.removeFromRight (180));
    }
    col.removeFromTop (6);
    {
        auto row = col.removeFromTop (28);
        metronomeToggle.setBounds (row.removeFromLeft (92));
        row.removeFromLeft (8);
        bpmLabel.setBounds (row.removeFromLeft (36));
        tapButton.setBounds (row.removeFromRight (56));
        row.removeFromRight (8);
        bpmSlider.setBounds (row);
    }
    col.removeFromTop (6);
    {
        auto row = col.removeFromTop (28);
        signatureLabel.setBounds (row.removeFromLeft (72));
        signatureCombo.setBounds (row.removeFromLeft (76));
        row.removeFromLeft (16);
        countInLabel.setBounds (row.removeFromLeft (70));
        countInCombo.setBounds (row);
    }
    col.removeFromTop (6);
    {
        auto row = col.removeFromTop (28);
        metronomeVolumeLabel.setBounds (row.removeFromLeft (72));
        metronomeVolumeSlider.setBounds (row);
    }

    // ================= coluna direita: Chrome =================
    col = rightColumn;

    chromeHeader.setBounds (col.removeFromTop (18));
    col.removeFromTop (6);
    chromeToggle.setBounds (col.removeFromTop (26).removeFromLeft (260));
    col.removeFromTop (6);
    {
        auto row = col.removeFromTop (28);
        chromeProcessLabel.setBounds (row.removeFromLeft (70));
        chromeProcessEditor.setBounds (row.removeFromLeft (160));
    }
    col.removeFromTop (6);
    chromeStatusLabel.setBounds (col.removeFromTop (22));
    col.removeFromTop (8);
    chromeTipLabel.setBounds (col.removeFromTop (100));
}

// ---------------------------------------------------------------------------
// Mixer e entradas (F5)
// ---------------------------------------------------------------------------

void MainComponent::setupMixer()
{
    const ChannelStrip::Config configs[numTracks] = {
        { "Bateria", theme::trackDrums, true, true, false },
        { "Chrome", theme::trackChrome, true, true, false },
        { "Entrada 1", theme::trackInput1, false, true, true },
        { "Entrada 2", theme::trackInput2, false, true, true },
    };

    for (int t = 0; t < numTracks; ++t)
    {
        strips[t] = std::make_unique<ChannelStrip> (configs[t]);
        addAndMakeVisible (*strips[t]);
        const juce::String key (trackKeys[t]);

        strips[t]->onGain = [this, t] (double db) { engine.setTrackGain (t, dbToGain (db)); };
        strips[t]->onGainCommitted = [this, t, key]
        {
            settings.set ("mixer", key + "Db", strips[t]->getGainDb());
            settings.save();
        };
        strips[t]->onMute = [this, t, key] (bool on)
        {
            engine.setTrackMute (t, on);
            settings.set ("mixer", key + "Mudo", on);
            settings.save();
        };
        strips[t]->onSolo = [this, t, key] (bool on)
        {
            engine.setTrackSolo (t, on);
            settings.set ("mixer", key + "Solo", on);
            settings.save();
        };

        if (t >= trackInput1)
        {
            const int k = t - trackInput1;
            strips[t]->onArm = [this, key] (bool on)
            {
                settings.set ("mixer", key + "Gravar", on);
                settings.save();
                if (on && recorder.getState() == Recorder::State::recording)
                    setStatus (L"A entrada armada entra na próxima gravação.");
            };
            strips[t]->onMonitor = [this, k, key] (bool on)
            {
                engine.setInputMonitor (k, on);
                settings.set ("mixer", key + "Ouvir", on);
                settings.save();
            };
        }
    }

    strips[trackDrums]->setCaption ("SSD5");
    strips[trackChrome]->setCaption ("Songsterr");

    masterStrip = std::make_unique<ChannelStrip> (ChannelStrip::Config { "Master", theme::accent, true, false, false });
    masterStrip->setCaption ("Behringer");
    addAndMakeVisible (*masterStrip);
    masterStrip->onGain = [this] (double db) { engine.setMasterGain (dbToGain (db)); };
    masterStrip->onGainCommitted = [this]
    {
        settings.set ("mixer", "masterDb", masterStrip->getGainDb());
        settings.save();
    };

    compensationSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    compensationSlider.setRange (-50.0, 50.0, 0.5);
    compensationSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
    compensationSlider.setTextValueSuffix (" ms");
    compensationSlider.setDoubleClickReturnValue (true, 0.0);
    compensationSlider.setTooltip (L"Se a guitarra gravada ficar atrasada em relação à bateria, aumente; adiantada, diminua.");
    compensationSlider.onValueChange = [this] { updateCompensationInfo(); };
    compensationSlider.onDragEnd = [this]
    {
        settings.set ("entradas", "ajusteMs", compensationSlider.getValue());
        settings.save();
    };
}

void MainComponent::loadMixerSettings()
{
    // Valores das versões anteriores (F1–F4) viram o padrão do mixer.
    const double legacyDb[numTracks] = {
        (double) settings.get ("saida", "volumeBateriaDb", 0.0),
        (double) settings.get ("chrome", "volumeDb", 0.0),
        0.0, 0.0
    };
    const bool legacyMute[numTracks] = { false, (bool) settings.get ("chrome", "mudo", false), false, false };

    for (int t = 0; t < numTracks; ++t)
    {
        const juce::String key (trackKeys[t]);
        const double db = (double) settings.get ("mixer", key + "Db", legacyDb[t]);
        const bool mute = (bool) settings.get ("mixer", key + "Mudo", legacyMute[t]);
        const bool solo = (bool) settings.get ("mixer", key + "Solo", false);

        strips[t]->setGainDb (db);
        strips[t]->setMute (mute);
        strips[t]->setSolo (solo);
        engine.setTrackGain (t, dbToGain (db));
        engine.setTrackMute (t, mute);
        engine.setTrackSolo (t, solo);

        if (t >= trackInput1)
        {
            const int k = t - trackInput1;
            const bool arm = (bool) settings.get ("mixer", key + "Gravar", false);
            const bool monitor = (bool) settings.get ("mixer", key + "Ouvir", false);
            strips[t]->setArm (arm);
            strips[t]->setMonitor (monitor);
            engine.setInputMonitor (k, monitor);
            engine.setInputChannel (k, k);
        }
    }

    const double masterDb = (double) settings.get ("mixer", "masterDb", 0.0);
    masterStrip->setGainDb (masterDb);
    engine.setMasterGain (dbToGain (masterDb));

    compensationSlider.setValue ((double) settings.get ("entradas", "ajusteMs", 0.0), juce::dontSendNotification);
}

void MainComponent::updateInputCaptions()
{
    auto* device = deviceManager.getCurrentAudioDevice();
    juce::StringArray activeNames;

    if (device != nullptr)
    {
        const auto names = device->getInputChannelNames();
        const auto active = device->getActiveInputChannels();
        for (int i = 0; i < names.size(); ++i)
            if (active[i])
                activeNames.add (names[i]);
    }

    for (int k = 0; k < numInputTracks; ++k)
        strips[trackInput1 + k]->setCaption (k < activeNames.size() ? activeNames[k]
                                                                    : juce::String (L"sem canal (ative em Áudio...)"));
    updateCompensationInfo();
}

int MainComponent::inputCompensationSamples() const
{
    auto* device = deviceManager.getCurrentAudioDevice();
    if (device == nullptr)
        return 0;

    const double sr = device->getCurrentSampleRate();
    const int total = device->getInputLatencyInSamples() + device->getOutputLatencyInSamples()
                    + juce::roundToInt (compensationSlider.getValue() * sr / 1000.0);
    return juce::jmax (0, total);
}

void MainComponent::updateCompensationInfo()
{
    auto* device = deviceManager.getCurrentAudioDevice();
    if (device == nullptr)
    {
        compensationInfo.setText ({}, juce::dontSendNotification);
        return;
    }

    const double sr = device->getCurrentSampleRate();
    juce::String text = L"Entradas gravadas adiantadas em\n";
    text << "entrada " << ms (device->getInputLatencyInSamples(), sr)
         << " + " << L"saída " << ms (device->getOutputLatencyInSamples(), sr);
    if (std::abs (compensationSlider.getValue()) > 0.01)
        text << (compensationSlider.getValue() > 0 ? juce::String (" + ") : juce::String (L" − "))
             << juce::String (std::abs (compensationSlider.getValue()), 1).replaceCharacter ('.', ',') << " ms";
    text << "\n= " << ms (inputCompensationSamples(), sr);
    compensationInfo.setText (text, juce::dontSendNotification);
}

// ---------------------------------------------------------------------------
// Mapeamento (F2)
// ---------------------------------------------------------------------------

void MainComponent::loadCatalog (const juce::File& kitFile)
{
    if (auto fromKit = DrumCatalog::fromSsd5KitFile (kitFile))
    {
        catalog = *fromKit;
        settings.set ("ssd5", "kit", kitFile.getFullPathName());
        settings.save();
    }
    else
    {
        catalog = DrumCatalog::builtInSsd5();
    }

    if (mappingWindow != nullptr)
        mappingWindow->refresh();
}

void MainComponent::chooseKitFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Escolha um kit do SSD5 (.ssd5kit)",
                                                       DrumCatalog::defaultSsd5KitFile().getParentDirectory(),
                                                       "*.ssd5kit");

    juce::Component::SafePointer<MainComponent> safe (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (safe == nullptr || file == juce::File())
            return;

        if (DrumCatalog::fromSsd5KitFile (file))
        {
            safe->loadCatalog (file);
            safe->setStatus (L"Peças lidas de " + file.getFileNameWithoutExtension() + ".");
        }
        else
        {
            safe->setStatus (L"Não encontrei peças em " + file.getFileName(), true);
        }
    });
}

void MainComponent::onProfileChanged()
{
    const auto& p = profiles.current();

    midiRouter.setMapping (p.compile());
    settings.set ("midi", "perfil", p.name);
    settings.save();

    refreshProfileCombo();
    updateGlobalHotkeys();

    if (mappingWindow != nullptr)
        mappingWindow->refresh();
}

void MainComponent::refreshProfileCombo()
{
    profileCombo.clear (juce::dontSendNotification);
    const auto names = profiles.names();
    for (int i = 0; i < names.size(); ++i)
        profileCombo.addItem (names[i], i + 1);
    profileCombo.setText (profiles.current().name, juce::dontSendNotification);
}

void MainComponent::updateGlobalHotkeys()
{
    std::vector<GlobalHotkeys::Binding> bindings;
    const auto& p = profiles.current();

    if (p.globalCommandKeys)
        for (int i = 0; i < p.keys.size(); ++i)
        {
            const auto& k = p.keys.getReference (i);
            const auto key = juce::KeyPress::createFromDescription (k.key);
            if (k.command != Command::none && GlobalHotkeys::canRegister (key))
                bindings.push_back ({ i, key });
        }

    hotkeys.setBindings (bindings);
}

void MainComponent::runCommand (Command c)
{
    juce::Logger::writeToLog ("comando: " + commandId (c));

    switch (c)
    {
        case Command::panic:
            releaseHeldKeys();
            engine.panic();
            setStatus (L"Pânico: todas as notas paradas.");
            break;

        case Command::record: toggleRecord();  break;
        case Command::play:   togglePlay();    break;
        case Command::stop:   stopTransport(); break;
        case Command::metronome:
            metronomeToggle.setToggleState (! metronomeToggle.getToggleState(), juce::sendNotification);
            break;

        case Command::none:
            break;
    }
}

void MainComponent::toggleMappingWindow()
{
    if (mappingWindow != nullptr)
    {
        mappingWindow->toFront (true);
        return;
    }

    mappingWindow = std::make_unique<MappingWindow> (mappingContext);
    mappingWindow->setHotkeyFailures (hotkeyFailures);

    juce::Component::SafePointer<MainComponent> safe (this);
    mappingWindow->onClose = [safe]
    {
        juce::MessageManager::callAsync ([safe]
        {
            if (safe != nullptr)
                safe->mappingWindow.reset();
        });
    };
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    const auto& p = profiles.current();

    for (const auto& b : p.keys)
    {
        if (b.key.isEmpty() || juce::KeyPress::createFromDescription (b.key) != key)
            continue;

        if (b.command != Command::none)
        {
            // Com atalhos globais ligados, o RegisterHotKey já entrega o comando (a tecla nem chega aqui).
            if (! p.globalCommandKeys)
                runCommand (b.command);
            return true;
        }

        const int code = key.getKeyCode();
        if (heldKeys.count (code) == 0) // ignora a repetição automática da tecla segurada
        {
            heldKeys[code] = b.note;
            engine.playNote (b.note, b.velocity, true);
            lastNoteLabel.setText (L"Tecla " + b.key, juce::dontSendNotification);
            lastNoteDetail.setText (L"→ SSD5: " + catalog.nameFor (b.note) + " (" + juce::String (b.note) + L")  ·  força "
                                        + juce::String (b.velocity),
                                    juce::dontSendNotification);
            noteFlash = 1.0f;
        }
        return true;
    }

    return false;
}

bool MainComponent::keyStateChanged (bool)
{
    for (auto it = heldKeys.begin(); it != heldKeys.end();)
    {
        if (! juce::KeyPress::isKeyCurrentlyDown (it->first))
        {
            engine.playNote (it->second, 0, false);
            it = heldKeys.erase (it);
        }
        else
        {
            ++it;
        }
    }
    return false;
}

void MainComponent::releaseHeldKeys()
{
    for (const auto& [code, note] : heldKeys)
        engine.playNote (note, 0, false);
    heldKeys.clear();
}

// ---------------------------------------------------------------------------
// Gravação (F3)
// ---------------------------------------------------------------------------

juce::File MainComponent::recordingsDirectory() const
{
    const auto saved = settings.get ("gravacao", "pasta").toString();
    return saved.isNotEmpty() ? juce::File (saved) : Recorder::defaultRecordingsDirectory();
}

void MainComponent::toggleRecord()
{
    const auto state = recorder.getState();

    if (state == Recorder::State::recording)
    {
        stopTransport();
        return;
    }

    if (state != Recorder::State::idle)
    {
        setStatus (L"Pare a reprodução antes de gravar.", true);
        return;
    }

    Recorder::Metadata meta;
    meta.profile = profiles.current().name;
    meta.kit = catalog.sourceDescription;
    meta.midiTiming = engine.getMidiTiming() == AudioEngine::MidiTiming::steady ? "estavel" : "rapido";
    meta.recordChrome = chromeCapture.isEnabled();
    meta.chromeProcess = chromeCapture.getExeName();
    meta.inputCompensationSamples = inputCompensationSamples();
    meta.bpm = engine.getTempo();
    meta.beatsPerBar = engine.getBeatsPerBar();
    meta.countInBars = juce::jmax (0, countInCombo.getSelectedId() - 1);
    for (int k = 0; k < numInputTracks; ++k)
    {
        auto& strip = *strips[trackInput1 + k];
        meta.recordInput[k] = strip.isArmed();
        meta.inputName[k] = strip.getName();
    }
    if (auto* p = engine.getPlugin())
    {
        meta.plugin = p->getName();
        meta.pluginLatencySamples = p->getLatencySamples();
    }

    juce::String error;
    if (! recorder.startRecording (recordingsDirectory(), meta, error))
        setStatus (error, true);

    updateTransportUi();
}

void MainComponent::togglePlay()
{
    const auto state = recorder.getState();

    if (state == Recorder::State::playing)
    {
        stopTransport();
        return;
    }

    if (state != Recorder::State::idle)
    {
        setStatus (L"Pare a gravação antes de tocar.", true);
        return;
    }

    juce::String error;
    if (! recorder.startPlayback (error))
        setStatus (error, true);

    updateTransportUi();
}

void MainComponent::stopTransport()
{
    recorder.stop();
    updateTransportUi();
}

void MainComponent::updateTransportUi()
{
    const auto state = recorder.getState();
    const auto* take = recorder.getLastTake();

    const auto formatTime = [] (double seconds)
    {
        const int tenths = juce::jmax (0, (int) std::floor (seconds * 10.0));
        return juce::String::formatted ("%02d:%02d.%d", tenths / 600, (tenths / 10) % 60, tenths % 10);
    };

    const auto red = theme::danger, green = theme::ok;
    recordButton.setToggleState (state == Recorder::State::recording, juce::dontSendNotification);
    playButton.setToggleState (state == Recorder::State::playing, juce::dontSendNotification);
    recordButton.setEnabled (state == Recorder::State::idle || state == Recorder::State::recording);
    playButton.setEnabled ((state == Recorder::State::idle && take != nullptr) || state == Recorder::State::playing);
    stopButton.setEnabled (state == Recorder::State::recording || state == Recorder::State::playing);

    // O andamento e o compasso vão para o .mid: ficam travados enquanto grava.
    const bool lockTempo = state == Recorder::State::recording || state == Recorder::State::finishing;
    for (auto* c : std::initializer_list<juce::Component*> { &bpmSlider, &tapButton, &signatureCombo, &countInCombo })
        c->setEnabled (! lockTempo);
    exportButton.setEnabled (take != nullptr && exportJob == nullptr && state == Recorder::State::idle);
    exportOtherButton.setEnabled (exportJob == nullptr && state == Recorder::State::idle);

    switch (state)
    {
        case Recorder::State::recording:
        {
            if (recorder.isCountingIn())
            {
                const int bpb = engine.getBeatsPerBar();
                const int total = juce::jmax (1, countInCombo.getSelectedId() - 1) * bpb;
                const auto index = engine.getMetronomeBeat() + total; // 0 = primeiro tempo da contagem
                timeLabel.setText (formatTime (0.0), juce::dontSendNotification);
                timeLabel.setColour (juce::Label::textColourId, theme::warn);
                transportInfoLabel.setText (index >= 0 && index < total
                                                ? L"Contagem: " + juce::String ((int) index % bpb + 1) + L" · compasso "
                                                      + juce::String ((int) index / bpb + 1) + " de " + juce::String (total / bpb)
                                                : juce::String (L"Contagem..."),
                                            juce::dontSendNotification);
                break;
            }

            timeLabel.setText (formatTime (recorder.getPositionSeconds()), juce::dontSendNotification);
            timeLabel.setColour (juce::Label::textColourId, red.brighter (0.4f));
            const int disk = juce::roundToInt (recorder.getDiskFillLevel() * 100.0f);
            transportInfoLabel.setText (L"● Gravando · " + juce::String (recorder.getRecordingNoteCount()) + L" notas"
                                            + (disk > 20 ? L" · fila do disco " + juce::String (disk) + "%" : juce::String()),
                                        juce::dontSendNotification);
            break;
        }

        case Recorder::State::playing:
            timeLabel.setText (formatTime (recorder.getPositionSeconds()), juce::dontSendNotification);
            timeLabel.setColour (juce::Label::textColourId, green.brighter (0.4f));
            transportInfoLabel.setText (L"▶ Tocando o último take pelo SSD5", juce::dontSendNotification);
            break;

        case Recorder::State::finishing:
            transportInfoLabel.setText (L"Salvando...", juce::dontSendNotification);
            break;

        case Recorder::State::idle:
            timeLabel.setColour (juce::Label::textColourId, theme::text);
            if (take != nullptr)
            {
                timeLabel.setText (formatTime (take->durationSec), juce::dontSendNotification);
                transportInfoLabel.setText (L"Último take: " + take->folder.getFileName() + L" · "
                                                + juce::String (take->noteCount) + " notas",
                                            juce::dontSendNotification);
            }
            else
            {
                timeLabel.setText (formatTime (0.0), juce::dontSendNotification);
                transportInfoLabel.setText (L"Nada gravado ainda. F9 grava, F8 para.", juce::dontSendNotification);
            }
            break;
    }

    folderLabel.setText (L"Pasta: " + recordingsDirectory().getFullPathName(), juce::dontSendNotification);
}

void MainComponent::chooseRecordingsFolder()
{
    fileChooser = std::make_unique<juce::FileChooser> (L"Pasta das gravações", recordingsDirectory());

    juce::Component::SafePointer<MainComponent> safe (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safe] (const juce::FileChooser& fc)
    {
        const auto dir = fc.getResult();
        if (safe == nullptr || dir == juce::File())
            return;

        safe->settings.set ("gravacao", "pasta", dir.getFullPathName());
        safe->settings.save();
        safe->recorder.loadLatestTake (dir);
        safe->updateTransportUi();
    });
}

// ---------------------------------------------------------------------------
// Metrônomo, contagem e exportação (F6)
// ---------------------------------------------------------------------------

void MainComponent::setupMetronome()
{
    metronomeToggle.setTooltip (L"Liga o clique do metrônomo (F7). A contagem antes de gravar toca mesmo com ele desligado.");

    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setRange (30.0, 300.0, 1.0);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 22);
    bpmSlider.setDoubleClickReturnValue (true, 120.0);
    bpmSlider.setTooltip (L"Andamento do metrônomo. Vai para o bateria.mid (a grade do FL Studio bate com o take).");
    bpmSlider.onValueChange = [this] { engine.setTempo (bpmSlider.getValue()); };
    bpmSlider.onDragEnd = [this] { saveMetronomeSettings(); };

    tapButton.setTooltip (L"Clique no ritmo da música (4 vezes ou mais) para achar o BPM");
    tapButton.onClick = [this] { tapTempo(); };

    for (int n = 2; n <= 7; ++n)
        signatureCombo.addItem (juce::String (n) + "/4", n);
    signatureCombo.onChange = [this]
    {
        engine.setBeatsPerBar (signatureCombo.getSelectedId());
        saveMetronomeSettings();
    };

    countInCombo.addItem ("Sem contagem", 1);
    countInCombo.addItem ("1 compasso", 2);
    countInCombo.addItem ("2 compassos", 3);
    countInCombo.setTooltip (L"Compassos de clique antes de a gravação começar (F9). O take começa no tempo 1.");
    countInCombo.onChange = [this] { saveMetronomeSettings(); };

    metronomeVolumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    metronomeVolumeSlider.setRange (-40.0, 0.0, 0.5);
    metronomeVolumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
    metronomeVolumeSlider.setTextValueSuffix (" dB");
    metronomeVolumeSlider.setDoubleClickReturnValue (true, -6.0);
    metronomeVolumeSlider.setTooltip (L"Volume do clique (passa pelo Master; nunca vai para a gravação)");
    metronomeVolumeSlider.onValueChange = [this] { engine.setMetronomeGain (dbToGain (metronomeVolumeSlider.getValue())); };
    metronomeVolumeSlider.onDragEnd = [this] { saveMetronomeSettings(); };

    metronomeToggle.onClick = [this]
    {
        engine.setMetronomeOn (metronomeToggle.getToggleState());
        saveMetronomeSettings();
    };
}

void MainComponent::loadMetronomeSettings()
{
    const bool on = (bool) settings.get ("metronomo", "ligado", false);
    const double bpm = juce::jlimit (30.0, 300.0, (double) settings.get ("metronomo", "bpm", 120.0));
    const int bpb = juce::jlimit (2, 7, (int) settings.get ("metronomo", "compasso", 4));
    const int countIn = juce::jlimit (0, 2, (int) settings.get ("metronomo", "contagem", 1));
    const double volumeDb = juce::jlimit (-40.0, 0.0, (double) settings.get ("metronomo", "volumeDb", -6.0));

    metronomeToggle.setToggleState (on, juce::dontSendNotification);
    bpmSlider.setValue (bpm, juce::dontSendNotification);
    signatureCombo.setSelectedId (bpb, juce::dontSendNotification);
    countInCombo.setSelectedId (countIn + 1, juce::dontSendNotification);
    metronomeVolumeSlider.setValue (volumeDb, juce::dontSendNotification);

    engine.setMetronomeOn (on);
    engine.setTempo (bpm);
    engine.setBeatsPerBar (bpb);
    engine.setMetronomeGain (dbToGain (volumeDb));
}

void MainComponent::saveMetronomeSettings()
{
    settings.set ("metronomo", "ligado", metronomeToggle.getToggleState());
    settings.set ("metronomo", "bpm", bpmSlider.getValue());
    settings.set ("metronomo", "compasso", signatureCombo.getSelectedId());
    settings.set ("metronomo", "contagem", juce::jmax (0, countInCombo.getSelectedId() - 1));
    settings.set ("metronomo", "volumeDb", metronomeVolumeSlider.getValue());
    settings.save();
}

void MainComponent::tapTempo()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (! tapTimes.empty() && now - tapTimes.back() > 2.0)
        tapTimes.clear(); // pausa longa: recomeça
    tapTimes.push_back (now);
    if (tapTimes.size() > 8)
        tapTimes.erase (tapTimes.begin());

    if (tapTimes.size() < 3)
    {
        setStatus (L"Tap: continue batendo no ritmo...");
        return;
    }

    const double interval = (tapTimes.back() - tapTimes.front()) / (double) (tapTimes.size() - 1);
    const double bpm = juce::jlimit (30.0, 300.0, std::round (60.0 / interval));
    bpmSlider.setValue (bpm, juce::sendNotificationSync);
    saveMetronomeSettings();
    setStatus ("Tap: " + juce::String ((int) bpm) + " BPM");
}

void MainComponent::updateMetronomeUi()
{
    const int bpb = engine.getBeatsPerBar();
    const auto beat = engine.getMetronomeBeat();
    const int inBar = (int) (((beat % bpb) + bpb) % bpb);
    beatIndicator.setState (bpb, inBar, engine.getTransportState() == TransportState::countIn, engine.isMetronomeAudible());
}

mixexport::Gains MainComponent::currentMixGains() const
{
    mixexport::Gains g;
    bool anySolo = false;
    for (int t = 0; t < numTracks; ++t)
        anySolo = anySolo || strips[t]->isSoloed();

    for (int t = 0; t < numTracks; ++t)
    {
        const bool audible = ! strips[t]->isMuted() && (! anySolo || strips[t]->isSoloed());
        g.track[t] = audible ? dbToGain (strips[t]->getGainDb()) : 0.0f;
    }
    g.master = dbToGain (masterStrip->getGainDb());
    return g;
}

void MainComponent::exportMix (const juce::File& takeFolder)
{
    if (exportJob != nullptr)
        return;

    exportJob = std::make_unique<ExportJob> (takeFolder, currentMixGains());
    setStatus (L"Exportando a mixagem de " + takeFolder.getFileName() + "...");
    updateTransportUi();
}

void MainComponent::chooseTakeToExport()
{
    const auto* take = recorder.getLastTake();
    fileChooser = std::make_unique<juce::FileChooser> (L"Take para exportar a mixagem",
                                                       take != nullptr ? take->folder : recordingsDirectory());

    juce::Component::SafePointer<MainComponent> safe (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safe] (const juce::FileChooser& fc)
    {
        const auto dir = fc.getResult();
        if (safe != nullptr && dir != juce::File())
            safe->exportMix (dir);
    });
}

void MainComponent::pollExport()
{
    if (exportJob == nullptr)
        return;

    if (! exportJob->done.load())
    {
        if (timerTicks % 5 == 0)
            setStatus (L"Exportando a mixagem... " + juce::String (juce::roundToInt (exportJob->progress.load() * 100.0f)) + "%");
        return;
    }

    const auto r = exportJob->result;
    const auto folder = exportJob->folder;
    exportJob.reset();

    if (r.ok)
    {
        juce::String text = L"Mixagem exportada: " + folder.getFileName() + "/" + r.file.getFileName() + " ("
                          + juce::String (r.tracksUsed) + (r.tracksUsed == 1 ? " faixa, " : " faixas, ")
                          + juce::String (r.seconds, 1).replaceCharacter ('.', ',') + " s)";
        if (r.appliedGain < 1.0f)
            text << L" · abaixada " << juce::String (juce::Decibels::gainToDecibels (r.appliedGain), 1).replaceCharacter ('.', ',')
                 << L" dB para não clipar";
        setStatus (text);
        juce::Logger::writeToLog ("mixagem exportada: " + r.file.getFullPathName());
        r.file.revealToUser();
    }
    else if (r.cancelled)
    {
        setStatus (L"Exportação cancelada.");
    }
    else
    {
        setStatus (r.error, true);
    }
    updateTransportUi();
}

void MainComponent::showAbout()
{
    juce::StringArray lines;
    for (const auto& k : profiles.current().keys)
        if (k.command != Command::none && k.key.isNotEmpty())
            lines.add (k.key + "|" + commandLabel (k.command));
    showAboutWindow (lines, settings.getDataDirectory());
}

// ---------------------------------------------------------------------------
// Chrome (F4)
// ---------------------------------------------------------------------------

void MainComponent::applyChromeSettings()
{
    auto exe = chromeProcessEditor.getText().trim();
    if (exe.isEmpty())
        exe = "chrome.exe";
    if (! exe.endsWithIgnoreCase (".exe"))
        exe << ".exe";
    chromeProcessEditor.setText (exe, false);

    const bool on = chromeToggle.getToggleState();
    settings.set ("chrome", "ativo", on);
    settings.set ("chrome", "processo", exe);
    settings.save();

    if (recorder.getState() == Recorder::State::recording && on != chromeCapture.isEnabled())
        setStatus (L"A captura do Chrome mudou durante a gravação: o chrome.wav só existe se ela estava ligada no início.");

    chromeCapture.setEnabled (on, exe);
    chromeStatusLabel.setText (chromeCapture.getStatusText(), juce::dontSendNotification);
}

} // namespace bb
