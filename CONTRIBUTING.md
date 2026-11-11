# Contribuir

Sugestões e correções são bem-vindas.

- **Bug ou ideia:** abra uma [issue](https://github.com/mateusvpassos/backbeat/issues/new/choose) usando o modelo.
- **Falha de segurança:** siga o [SECURITY.md](SECURITY.md), não abra issue pública.

## Pull requests

1. Para mudanças grandes, abra uma issue antes para combinar a ideia.
2. Compile e rode os testes (veja **Compilar** no [README](README.md)):
   ```powershell
   .\scripts\build.ps1 -Test
   ```
   Para não mexer nos seus dados reais, use uma pasta separada: `$env:BACKBEAT_DATA = "C:\temp\bb-dev"` antes de abrir o app, e troque a pasta de gravação para uma pasta temporária.
3. Respeite as regras da thread de áudio: nada de alocação, lock bloqueante, disco ou log dentro do callback. Comunicação por filas lock-free e atômicos.
4. Mantenha a interface em português. Textos com acento vão como `L"..."` (o `juce::String` trata `const char*` como ASCII).
5. Anote a mudança em `CHANGELOG.md`, na seção **Não lançado**.

Ao enviar um pull request, você concorda que a contribuição é licenciada sob a [GNU AGPLv3](LICENSE) do projeto.
