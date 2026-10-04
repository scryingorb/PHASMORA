param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
$localCmake = Join-Path $PSScriptRoot '.tools/cmake-package/cmake/data/bin/cmake.exe'
if (Test-Path -LiteralPath $localCmake) {
    $cmakeCommand = $localCmake
} else {
    $cmakeCommand = (Get-Command cmake -ErrorAction Stop).Source
}
$buildDirectory = Join-Path $PSScriptRoot 'build'
& $cmakeCommand -S $PSScriptRoot -B $buildDirectory -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& $cmakeCommand --build $buildDirectory --config $Configuration --target PHASMORA_VST3 --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'PHASMORA build failed.' }
