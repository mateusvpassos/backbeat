# Segurança

## Versões com correção

Só a versão mais recente recebe correções de segurança.

## Como reportar

**Não abra uma issue pública.** Use o reporte privado do GitHub: aba **Security → Report a vulnerability** ([link direto](https://github.com/mateusvpassos/backbeat/security/advisories/new)).

Inclua a versão do Backbeat e do Windows, os passos para reproduzir e o impacto. A resposta inicial vem em até 7 dias.

## Escopo

O Backbeat roda só no seu computador e não acessa a rede. Interessam principalmente:
- execução de código a partir de `settings.json`, de um perfil (`perfis\*.json`), de um `sessao.json` ou de um kit `.ssd5kit`;
- problemas no `install.ps1` ou nos atalhos criados por ele.

Plugins VST3 rodam dentro do processo do Backbeat com as mesmas permissões dele: carregue só plugins em que você confia. Falhas dentro de um plugin devem ser reportadas ao fabricante dele.
