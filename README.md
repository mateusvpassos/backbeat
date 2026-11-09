# Backbeat

"Mini FL Studio" para praticar e gravar bateria eletrônica no Windows: a bateria (Nexus, MIDI "EDRUM") toca o Steven Slate Drums 5 pelo ASIO da Behringer, com o menor atraso possível. C++ / JUCE 8 / CMake. Interface em português do Brasil.

## Estado

- **F1:** abre o ASIO da Behringer, carrega o SSD5, abre a interface dele, salva/restaura o estado (kit, mixer interno), toca com a EDRUM e mostra latência, CPU, falhas e o VU.
- **F2:** mapeamento. Janela **Mapeamento...**:
  - **Pads:** bata num pad, ele aparece na lista e você escolhe a peça do SSD5 (lista lida do próprio kit `.ssd5kit`, com os nomes do SSD5). Pad fora da lista: tocar como chega ou ignorar. Curva de força e força mínima. Pedal do chimbal: número do CC, inverter, faixa e calibração. O SSD5 abre o chimbal pelo CC4 nas peças "eD" (42 ponta, 22 corpo, 44 pedal).
  - **Teclado:** teclas do PC tocam peças (com o Backbeat em foco) ou disparam comandos (Gravar, Tocar, Parar, Pânico). Comandos podem ser **globais** (funcionam com o Songsterr na frente), via `RegisterHotKey`.
  - **Perfis:** novo, duplicar, renomear, excluir (vai pra Lixeira). Perfil inicial "Nexus (General MIDI)": corrige o que difere entre GM e SSD5 (ex.: GM 50 é tom, no SSD5 é splash).

- **F3:** gravação. **● Gravar** (F9), **▶ Tocar** (F10), **■ Parar** (F8). Cada take vira uma pasta `Música\Backbeat\AAAA-MM-DD HH-MM-SS\` (a pasta pode ser trocada em **Pasta...**):
  - `bateria.mid`: as notas que o SSD5 recebeu (depois do mapeamento), com o CC4 do chimbal, 120 BPM, 4/4. Note-off que a bateria não mandou é criado (50 ms) para o FL Studio não mostrar notas de duração zero.
  - `bateria.wav`: saída do SSD5, 24 bits estéreo, **alinhada amostra a amostra** com o `.mid` (mesmo contador de amostras do callback; a latência do plugin, se houver, é descontada). O volume da tela não afeta a gravação.
  - `sessao.json`: duração, taxa, perfil, kit, modo de tempo das notas e perdas (se o disco não deu conta).
  - **Tocar** repete o último take pelo SSD5 com o kit atual (dá para trocar o kit e ouvir de novo).
  - O WAV é gravado por uma thread própria a partir de um FIFO lock-free de 10 s; a thread de áudio não espera o disco.
  - Para gravar, prefira **Ritmo estável** em "Tempo das notas": no modo **Menor latência** cada nota cai no início do bloco seguinte (até ~6 ms de oscilação, igual no `.mid` e no `.wav`).

- **F4 (atual):** captura do Chrome (Songsterr) **sem cabo virtual nem Voicemeeter**, pelo loopback por processo do WASAPI (Windows 10 2004+). Seção **Chrome** na tela:
  - **Capturar o áudio do Chrome** liga/desliga; o processo pode ser trocado (ex.: `msedge.exe`, `spotify.exe`). Se o Chrome fechar e abrir de novo, a captura volta sozinha.
  - O áudio é pedido na mesma taxa do ASIO (o Windows converte 48 kHz → 44,1 kHz). A pequena diferença de relógio entre a Behringer e o Windows (medida aqui: ~30 ppm ≈ 0,1 s por hora) é corrigida por um resampler adaptativo: o atraso do Chrome fica fixo em ~40 ms por horas (teste simulado de 30 min: 38,5–41,5 ms, sem faltas).
  - Volume, mudo e VU próprios; sai pela Behringer junto com a bateria.
  - Gravando com a captura ligada, sai também `chrome.wav` (24 bits estéreo), **com o mesmo início e tamanho** do `bateria.wav`: as duas faixas ficam alinhadas como você ouviu.
  - **Para não ouvir em dobro:** Configurações › Sistema › Som › Mixer de volume › Chrome › Dispositivo de saída = uma saída que você não usa. A captura pega o Chrome em qualquer saída.

Próximas fases: F5 entradas da Behringer + mixer · F6 metrônomo, contagem, exportar mix e polimento.

### Teclas padrão (perfil Nexus)
| Tecla | Peça | | Tecla | Peça |
|---|---|---|---|---|
| `Z` | Kick | | `A` `S` `D` `F` | Tons 10" 12" 14" 16" |
| `X` | Snare Center | | `Q` / `W` | Crash L / Crash R |
| `C` / `V` | Chimbal fechado / aberto | | `E` / `R` | Ride / Ride Bell |
| `B` | Pedal do chimbal | | `F9` `F10` `F8` | Gravar · Tocar · Parar (globais) |

## Compilar

Pré-requisitos (já instalados nesta máquina):
- Visual Studio 2026 Build Tools com **Desenvolvimento para desktop com C++** (traz MSVC, Windows SDK e CMake)
- Git

```powershell
# 1ª vez: baixa o JUCE 8.0.15 (o CMake também baixa sozinho se a pasta não existir)
git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE.git external/JUCE

.\scripts\build.ps1 -Test     # compila (Release) e roda os testes
.\scripts\build.ps1 -Run      # compila e abre o app
```

O executável fica em `build\Backbeat_artefacts\Release\Backbeat.exe` (runtime estático, roda sozinho).

### ASIO
O JUCE 8.0.15 já traz os arquivos do **ASIO SDK da Steinberg** (licença GPLv3, liberada pela Steinberg em out/2025). O build só liga `JUCE_ASIO=1` no `CMakeLists.txt`, sem download separado. Para usar outra versão do SDK: `JUCE_ASIO_USE_EXTERNAL_SDK=1` e adicione a pasta `common/` do SDK aos includes.

## Antes de abrir
- **Voicemeeter:** o ASIO da Behringer só atende um programa por vez. Feche o Voicemeeter, ou tire a Behringer da saída A1 dele (ou use-a em WDM/MME), enquanto o Backbeat estiver aberto.
- **FL Studio** aberto com o ASIO da Behringer também bloqueia.

## Dados
Em `%APPDATA%\Backbeat\`:
- `settings.json`: dispositivo de áudio, caminho e estado do plugin (base64), entrada MIDI, modo de tempo, par de saídas, volume, perfil ativo, kit do SSD5 usado para os nomes das peças.
- `perfis\*.json`: perfis de mapeamento (pads, força, chimbal, teclas).
- `backbeat.log`: tempos de inicialização e erros de dispositivo/plugin.

Para testar sem mexer nos seus dados: `$env:BACKBEAT_DATA = "C:\temp\bb-teste"`. `Backbeat.exe --mapeamento` abre direto a janela de mapeamento.

## Licença
Para uso pessoal. Se for distribuir: JUCE (AGPLv3 ou licença JUCE) + ASIO SDK (GPLv3) exigem código aberto compatível (AGPLv3).
