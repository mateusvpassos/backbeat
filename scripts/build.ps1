# Compila o Backbeat com o CMake que vem no Visual Studio (não precisa de CMake no PATH).
#   .\scripts\build.ps1            -> Release
#   .\scripts\build.ps1 -Debug     -> Debug
#   .\scripts\build.ps1 -Test      -> Release + roda os testes
#   .\scripts\build.ps1 -Run       -> Release + abre o app
param(
    [switch]$Debug,
    [switch]$Test,
    [switch]$Run
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$config = if ($Debug) { 'Debug' } else { 'Release' }

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "Visual Studio não encontrado. Instale 'Desenvolvimento para desktop com C++' no Visual Studio 2022/2026 ou no Build Tools." }
$vsArgs = @('-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64')
$vs = & $vswhere @vsArgs -property installationPath
$vsMajor = "$(& $vswhere @vsArgs -property installationVersion)".Split('.')[0]
$generator = switch ($vsMajor) {
    '18' { 'Visual Studio 18 2026' }
    '17' { 'Visual Studio 17 2022' }
    default { throw "Visual Studio com C++ não encontrado (versão: '$vsMajor'). Use o 2022 ou o 2026." }
}

# CMake do próprio Visual Studio (conhece o gerador dele); senão o do PATH.
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) { $cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source }
if (-not $cmake) { throw "CMake não encontrado. Instale 'Desenvolvimento para desktop com C++' no Visual Studio." }

$build = Join-Path $root 'build'
if (-not (Test-Path (Join-Path $build 'CMakeCache.txt'))) {
    & $cmake -S $root -B $build -G $generator -A x64
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao configurar o CMake.' }
}

& $cmake --build $build --config $config --parallel -- /nologo /verbosity:minimal
if ($LASTEXITCODE -ne 0) { throw 'Falha na compilação.' }

$exe = Join-Path $build "Backbeat_artefacts\$config\Backbeat.exe"
Write-Host "`nPronto: $exe"

if ($Test) {
    & (Join-Path $build "BackbeatTests_artefacts\$config\BackbeatTests.exe")
    if ($LASTEXITCODE -ne 0) { throw 'Testes falharam.' }
}

if ($Run) { Start-Process $exe }
