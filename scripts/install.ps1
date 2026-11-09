# Instala o Backbeat para o usuário atual (sem precisar de administrador):
#   - copia o Backbeat.exe (Release) para %LOCALAPPDATA%\Programs\Backbeat
#   - cria atalhos na área de trabalho e no menu Iniciar
# Uso:  .\scripts\install.ps1            (compila antes)
#       .\scripts\install.ps1 -NoBuild   (usa o build que já existe)
#       .\scripts\install.ps1 -Uninstall (remove programa e atalhos; seus dados em %APPDATA%\Backbeat ficam)
param(
    [switch]$NoBuild,
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$target = Join-Path $env:LOCALAPPDATA 'Programs\Backbeat'
$desktopLink = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Backbeat.lnk'
$startLink = Join-Path ([Environment]::GetFolderPath('Programs')) 'Backbeat.lnk'

if ($Uninstall) {
    if (Get-Process Backbeat -ErrorAction SilentlyContinue) { throw 'Feche o Backbeat antes de desinstalar.' }
    Remove-Item $desktopLink, $startLink -ErrorAction SilentlyContinue
    Remove-Item $target -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host 'Backbeat removido (dados em %APPDATA%\Backbeat foram mantidos).'
    return
}

if (-not $NoBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Falha na compilação.' }
}

$exe = Join-Path $root 'build\Backbeat_artefacts\Release\Backbeat.exe'
if (-not (Test-Path $exe)) { throw "Não encontrei $exe. Rode sem -NoBuild." }

if (Get-Process Backbeat -ErrorAction SilentlyContinue) { throw 'Feche o Backbeat antes de instalar a nova versão.' }

New-Item -ItemType Directory -Force $target | Out-Null
Copy-Item $exe (Join-Path $target 'Backbeat.exe') -Force
Copy-Item (Join-Path $root 'README.md') (Join-Path $target 'LEIA-ME.md') -Force

$shell = New-Object -ComObject WScript.Shell
foreach ($link in @($desktopLink, $startLink)) {
    $s = $shell.CreateShortcut($link)
    $s.TargetPath = Join-Path $target 'Backbeat.exe'
    $s.WorkingDirectory = $target
    $s.Description = 'Backbeat: bateria eletrônica + SSD5 via ASIO'
    $s.IconLocation = (Join-Path $target 'Backbeat.exe') + ',0'
    $s.Save()
}

Write-Host "Instalado em $target"
Write-Host "Atalhos: $desktopLink"
Write-Host "         $startLink"
