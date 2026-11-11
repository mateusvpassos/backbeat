# Backbeat

Mini DAW para praticar e gravar bateria eletrônica no Windows. A bateria (MIDI) toca um plugin de bateria VST3 (feito para o **Steven Slate Drums 5**, aceita qualquer VST3) pelo ASIO da sua interface, com o menor atraso possível. Junto, ele captura o áudio do navegador (Songsterr, YouTube...) sem cabo virtual e grava tudo em faixas alinhadas amostra a amostra, prontas para abrir numa DAW.

C++20 / JUCE 8 / CMake. Interface em português do Brasil.

*Electronic drum practice and recording host for Windows: e-drum MIDI into a VST3 drum plugin over ASIO, per-process browser audio capture, sample-accurate multitrack takes. The interface is in Brazilian Portuguese.*

## Requisitos

- Windows 10 2004 ou mais novo (a captura por processo usa o loopback do WASAPI dessa versão)
- Interface de áudio com driver **ASIO**
- Bateria eletrônica ou teclado com saída MIDI (USB)
- Um plugin de bateria **VST3** (o SSD5 não acompanha o Backbeat)

## Recursos

- **Plugin de bateria:** **Trocar plugin** lista os .vst3 instalados (`Common Files\VST3`, a pasta VST3 do usuário e as pastas que você adicionar em **Adicionar pasta de VST3...**) ou abre outro arquivo. Cada plugin guarda o seu estado (kit, mixer interno). Mostra latência, CPU, falhas e VU.
- **Mapeamento** (janela **Mapeamento...**):
  - **Pads:** bata num pad e escolha a peça. Com o SSD5 a lista vem do próprio kit `.ssd5kit`; com outro plugin, da tabela General MIDI. Curva de força, força mínima e pedal do chimbal (CC, inverter, faixa, calibração).
  - **Teclado:** teclas do PC tocam peças ou disparam comandos (Gravar, Tocar, Parar, Pânico, Metrônomo). Comandos podem ser **globais** e funcionar com o navegador na frente.
  - **Perfis:** novo, duplicar, renomear, excluir. O perfil inicial "Nexus (General MIDI)" corrige o que difere entre GM e SSD5 (ex.: GM 50 é tom, no SSD5 é splash).
- **Gravação:** **● Gravar** (F9), **▶ Tocar** (F10), **■ Parar** (F8). Cada take vira uma pasta `Música\Backbeat\AAAA-MM-DD HH-MM-SS\` (a pasta pode ser trocada) com:
  - `bateria.mid`: as notas que o plugin recebeu (já mapeadas), com o CC4 do chimbal, o andamento e o compasso do metrônomo;
  - `bateria.wav`: saída do plugin, 24 bits estéreo, alinhada amostra a amostra com o `.mid`;
  - `chrome.wav`, `entrada-1.wav`, `entrada-2.wav`: as outras faixas, com o mesmo início e tamanho;
  - `sessao.json`: duração, taxa, perfil, kit, andamento e perdas (se o disco não deu conta).

  **Tocar** repete o último take pelo plugin com o kit atual. Para gravar, prefira **Ritmo estável** em "Tempo das notas" (no modo **Menor latência** cada nota cai no início do bloco seguinte).
- **Captura do navegador sem cabo virtual nem Voicemeeter:** loopback por processo do WASAPI. O processo pode ser trocado (`chrome.exe`, `msedge.exe`, `spotify.exe`...) e a captura volta sozinha se ele reabrir. A diferença de relógio entre a interface e o Windows é corrigida por um resampler adaptativo: o atraso fica fixo em ~40 ms por horas.
  **Para não ouvir em dobro:** Configurações › Sistema › Som › Mixer de volume › Chrome › Dispositivo de saída = uma saída que você não usa. A captura pega o processo em qualquer saída.
- **Mixer:** uma tira por faixa (Bateria, Chrome, Entrada 1, Entrada 2, Master) com fader em dB, VU, mudo e solo. Volume, mudo e solo só mudam o que você ouve: a gravação é sempre antes do fader.
- **Entradas da interface** (mic/guitarra nos canais 1 e 2): armar para gravar e ouvir pelo Backbeat. São gravadas adiantadas pela latência de entrada + saída (com ajuste fino), para ficarem no tempo de quem tocou junto.
- **Metrônomo:** 30–300 BPM, **Tap**, compasso 2/4 a 7/4, volume próprio. Nunca vai para a gravação. **Contagem** de 1 ou 2 compassos antes de gravar; o take começa exatamente no tempo 1.
- **Exportar mixagem:** junta as faixas de um take em `mixagem.wav` (24 bits) com os volumes atuais do mixer; se passar de 0 dBFS, normaliza o pico em −1 dBFS.

### Teclas padrão (perfil Nexus)
| Tecla | Peça | | Tecla | Peça |
|---|---|---|---|---|
| `Z` | Kick | | `A` `S` `D` `F` | Tons 10" 12" 14" 16" |
| `X` | Snare Center | | `Q` / `W` | Crash L / Crash R |
| `C` / `V` | Chimbal fechado / aberto | | `E` / `R` | Ride / Ride Bell |
| `B` | Pedal do chimbal | | `F9` `F10` `F8` | Gravar · Tocar · Parar (globais) |
| | | | `F7` | Metrônomo liga/desliga (global) |

## Antes de abrir

Drivers ASIO costumam atender um programa por vez. Feche o que estiver usando a interface pelo ASIO (Voicemeeter, uma DAW aberta...) enquanto o Backbeat estiver aberto.

## Compilar

Pré-requisitos:
- Visual Studio 2022 ou 2026 (ou Build Tools) com **Desenvolvimento para desktop com C++** (traz MSVC, Windows SDK e CMake)
- Git

```powershell
# Opcional: clone local do JUCE (sem ele, o CMake baixa sozinho no primeiro build)
git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE.git external/JUCE

.\scripts\build.ps1 -Test     # compila (Release) e roda os testes
.\scripts\build.ps1 -Run      # compila e abre o app
```

O executável fica em `build\Backbeat_artefacts\Release\Backbeat.exe` (runtime estático, roda sozinho). Se o PowerShell bloquear scripts, rode com `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test`.

O JUCE 8.0.15 já traz o **ASIO SDK da Steinberg** (GPLv3); o build só liga `JUCE_ASIO=1`, sem download separado.

### Instalar
```powershell
.\scripts\install.ps1             # compila, copia para %LOCALAPPDATA%\Programs\Backbeat e cria atalhos
.\scripts\install.ps1 -NoBuild    # instala o build que já existe
.\scripts\install.ps1 -Uninstall  # remove programa e atalhos (seus dados em %APPDATA%\Backbeat ficam)
```
Atalhos em `Área de Trabalho\Programs\` e no menu Iniciar. Não precisa de administrador. Feche o Backbeat antes de reinstalar.

## Dados e privacidade

O Backbeat funciona offline: não tem conta, não acessa a rede e não coleta dados de uso. Tudo fica em `%APPDATA%\Backbeat\`:
- `settings.json`: dispositivo de áudio, plugin atual, pastas extras de VST3 e o estado de cada plugin usado, entrada MIDI, mixer, entradas e metrônomo;
- `perfis\*.json`: perfis de mapeamento (pads, força, chimbal, teclas);
- `backbeat.log`: tempos de inicialização e erros de dispositivo/plugin.

Para testar sem mexer nos seus dados: `$env:BACKBEAT_DATA = "C:\temp\bb-teste"`. `Backbeat.exe --mapeamento` abre direto a janela de mapeamento.

## Contribuir e segurança

Veja [CONTRIBUTING.md](CONTRIBUTING.md) e as mudanças em [CHANGELOG.md](CHANGELOG.md). Falhas de segurança: [SECURITY.md](SECURITY.md) (não abra issue pública).

## Licença

[GNU AGPLv3](LICENSE), exigida pelo JUCE (AGPLv3) e pelo ASIO SDK (GPLv3). Fonte Geist (OFL 1.1), VST 3 SDK (MIT) e marcas citadas: ver [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
