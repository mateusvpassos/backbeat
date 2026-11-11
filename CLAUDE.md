# Backbeat

Host de bateria eletrônica: EDRUM (MIDI) → SSD5 (VST3) → Behringer (ASIO), com captura do Chrome (WASAPI loopback por processo), entradas da Behringer, mixer e gravação em faixas sincronizadas.

C++20 / JUCE 8.0.15 / CMake (gerador VS 2022 ou 2026, detectado pelo `build.ps1` via vswhere). Interface em português do Brasil. Repo público, licença AGPLv3 (obrigatória por causa do JUCE/ASIO, não MIT); mudanças vão no `CHANGELOG.md` (seção Não lançado).

## Comandos
```powershell
.\scripts\build.ps1 -Test   # compila Release + testes (BackbeatTests)
.\scripts\build.ps1 -Run    # compila e abre
```

## Regras
- **Thread de áudio** (`AudioEngine::audioDeviceIOCallbackWithContext` e tudo que ela chama): sem alocação, sem lock bloqueante (só `tryLock`), sem I/O, sem log. Comunicação por `SpscQueue` (um produtor, um consumidor) e atômicos.
- Disco sempre em thread separada (`DiskWriter`).
- O plugin só é trocado pela thread de mensagens via `AudioEngine::setPlugin`. A janela do plugin é destruída antes do plugin. Qualquer VST3: `PluginHost::findInstalledVst3 (pastasExtras)` (em `PluginFiles.cpp`, sem abrir plugins; compila nos testes; pastas extras em `plugin.pastas`) e estado por plugin em `plugin.estados` (lista caminho/estado). SSD5 = catálogo do .ssd5kit; outros = `DrumCatalog::builtInGm`.
- **Textos com acento:** `juce::String` trata `const char*` como ASCII (acento vira lixo). Use `L"..."` (o projeto compila com `/utf-8`) ou `juce::String::fromUTF8 ("...")`. Mensagens de `static_assert` sem acento.
- Não usar `juce::String::indexOf/substring` em laço sobre textos grandes (UTF-8, acesso linear): converter para `std::string`.
- Mapeamento: o perfil vira uma `MappingTable` (POD) publicada por `SeqLock` para a thread MIDI. Teclado do PC e botão "testar" entram por `AudioEngine::pushUiMidi` (fila separada; cada `SpscQueue` tem um produtor só).
- Gravação: `DiskWriter` (FIFO lock-free + thread que acorda sozinha; nunca `ThreadedWriter`, que chama `notify()` do áudio). Transporte por pedidos atômicos aplicados no início do bloco; recursos só são liberados depois de `takeResourcesReleased()`. Bloco sem plugin grava silêncio (o WAV nunca encurta). `tests/RecordingTests.cpp` roda o AudioEngine com dispositivo e plugin falsos e confere MIDI x WAV amostra a amostra.
- Chrome: `ProcessLoopbackCapture` (COM/WASAPI numa thread própria, formato na taxa do ASIO) → `LoopbackStream` (FIFO lock-free) → `DriftResampler` na thread de áudio (Lagrange + PI no nível do FIFO, alvo 40 ms). `AudioEngine::setChromeStream` usa handshake seq_cst; só destrua o stream depois que ele voltar. `tests/DriftTests.cpp` simula 30 min com deriva.
- Mixer (F5): faixas `TrackId` com ganho/mudo/solo atômicos; tudo soma no `mixBus` e sai pelo master. Gravação sempre antes do fader. Entradas gravadas adiantadas em `RecordTargets::inputCompensation` (entrada + saída + ajuste fino) e completadas com silêncio no fim pelo `Recorder`. Coberto em `tests/RecordingTests.cpp` (MixerTest).
- Metrônomo (F6): `AudioEngine::renderMetronome` soma o clique no `mixBus` (nunca gravado). Grade = transporte: `metroPos` 0 = amostra 0 do take. Contagem = `TransportState::countIn`, arredondada para blocos inteiros, e `beginRecording()` no início do bloco em que acaba. Mixagem: `rec/MixExporter` (fora do tempo real, numa `juce::Thread`). Coberto em `tests/MetronomeTests.cpp`.
- Visual: siga a família (Riff/Rubato/Stinger...). Cores em `ui/Theme.h` (mesmas variáveis do `style.css`, cor própria laranja-âmbar), componentes em `ui/BackbeatLookAndFeel.cpp`, fonte Geist embutida (`assets/fonts`, via `juce_add_binary_data`). Guia: skill `ui-style`. Botões: propriedade `variant` ("primary" = degradê, um por tela/"ghost"), `onColour` para o estado ligado e `icon` (nome Lucide de `ui/Icons.cpp`). Título de painel = `theme::header` (h2 14px) com ícone e linha desenhados por `paintPanelHead`; rótulo pequeno = `theme::lbl`. Barra de título nativa escura: `darkTitleBar()` (`ui/WindowChrome.cpp`, DWM) em toda janela nova. Textos do JUCE traduzidos em `portugueseStrings()` (`Main.cpp`). Ícone: `assets/icon.svg` (vira o ícone do .exe).
- Títulos em maiúsculas: `theme::upper()` (o `toUpperCase` do JUCE não converte ç/í).
- Teclas no Windows: o JUCE marca F1–F12/setas com o bit 0x10000; nunca converter minúscula→maiúscula sem checar esse bit (o F9 é 0x78 = 'x'). Coberto em `tests/HotkeyTests.cpp`.
- Testes de ponta a ponta no app: `BACKBEAT_DATA` temporário com `gravacao.pasta` também temporária, para não escrever em `Música\Backbeat`.
- Dados de teste isolados: `BACKBEAT_DATA=<pasta temporária>`. Nunca mexa no `%APPDATA%\Backbeat` real.
- `external/` (JUCE) e `build/` ficam fora do git.
- Git Bash: rodar scripts com `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Test` (política de execução bloqueia), e flags do MSBuild como `/nologo` viram caminho (use `MSYS_NO_PATHCONV=1`).

## Fluxo
Ao terminar uma mudança: `.\scripts\build.ps1 -Test`, conferir a tela (dados de teste isolados), commit + push na `main` (`mateusvpassos/backbeat`) e reinstalar com `.\scripts\install.ps1 -NoBuild` (o app precisa estar fechado).
- Smart App Control do Windows às vezes bloqueia um build novo ("Uma política de Controle de Aplicativo bloqueou este arquivo"): é por hash; outro build costuma passar. Não é bug do código.
