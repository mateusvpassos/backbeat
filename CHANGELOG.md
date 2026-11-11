# Changelog

Mudanças de cada versão do Backbeat. Formato baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/); as versões seguem [Versionamento Semântico](https://semver.org/lang/pt-BR/).

## [Não lançado]

### Adicionado
- **Trocar plugin › Adicionar pasta de VST3...**: pastas extras (ex.: `D:\VST3`) entram na lista de plugins e ficam salvas; **Remover pasta** tira da lista.
- Licença GNU AGPLv3, avisos de terceiros, guia de contribuição, política de segurança e modelos de issue.

### Alterado
- README reescrito para quem chega de fora (requisitos, recursos, compilar).
- `build.ps1` usa o Visual Studio instalado (2022 ou 2026), não só o 2026.
- `install.ps1` cria o atalho em `Área de Trabalho\Programs` em vez de solto na área de trabalho.
- Interface sem nomes do equipamento do autor (Behringer, EDRUM, Songsterr); a primeira execução sem o SSD5 pede para escolher um VST3.

## [1.0.0] - 2026-11-10

Versão de uso pessoal, publicada como código aberto em 2026-11-11.

### Adicionado
- Host de VST3 com ASIO: toca a bateria eletrônica (MIDI) num plugin de bateria (feito para o SSD5, aceita qualquer VST3), com estado salvo por plugin.
- Mapeamento de pads, curva de força, pedal do chimbal e teclas do PC (com atalhos globais), em perfis.
- Gravação sincronizada amostra a amostra: `bateria.mid`, `bateria.wav`, entradas da interface e `chrome.wav`.
- Captura do áudio de um processo (Chrome, Edge, Spotify...) pelo loopback do WASAPI, com correção de deriva de relógio.
- Mixer com volume, mudo e solo; compensação de latência nas entradas gravadas.
- Metrônomo, contagem antes de gravar e exportação da mixagem.
- Tela **Sobre** com versão, atalhos e licenças.

[Não lançado]: https://github.com/mateusvpassos/backbeat/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/mateusvpassos/backbeat/commits/main
