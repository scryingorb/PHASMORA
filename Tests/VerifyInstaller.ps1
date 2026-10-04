param()
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$testRoot=Join-Path $projectRoot 'build\Installer-Test'
$destination=Join-Path $testRoot 'PHASMORA.vst3'
$resolved=[IO.Path]::GetFullPath($destination)
$allowed=[IO.Path]::GetFullPath((Join-Path $projectRoot 'build'))+[IO.Path]::DirectorySeparatorChar
if (!$resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe controlled install target' }
$registry='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{E14C74E3-461B-48F2-AC84-D6F12F08D9F0}_is1'
if (Test-Path $registry) { throw 'Existing per-user GHOST registration; do not overwrite it for a test' }
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$sibling=Join-Path $testRoot 'UnrelatedPlugin.keep'
Set-Content -LiteralPath $sibling -Value 'preserve unrelated content'
$installer=Join-Path $projectRoot 'dist\PHASMORA-1.0.0-Windows\PHASMORA-1.0.0-Windows-Setup.exe'
$source=Join-Path $projectRoot 'build\GHOST_artefacts\Release\VST3\PHASMORA.vst3'
for ($pass=1;$pass -le 2;$pass++) {
    $installLog=Join-Path $testRoot "install-$pass.log"
    $arguments=@('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/CURRENTUSER',('/DIR="'+$destination+'"'),('/LOG="'+$installLog+'"'))
    $process=Start-Process -FilePath $installer -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Controlled installation failed: $($process.ExitCode)" }
    foreach ($file in Get-ChildItem -LiteralPath $source -File -Recurse) {
        $relative=$file.FullName.Substring($source.Length+1)
        $installed=Join-Path $destination $relative
        if (!(Test-Path -LiteralPath $installed)) { throw "Missing bundle file $relative" }
        if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $installed).Hash) { throw "Bundle mismatch $relative" }
    }
    Write-Output "Controlled installation pass ${pass}: complete bundle hashes match"
}
$uninstaller=Join-Path $destination 'Uninstall\unins000.exe'
if (!(Test-Path -LiteralPath $uninstaller)) { throw 'Uninstall support missing' }
$uninstallLog=Join-Path $testRoot 'uninstall.log'
$process=Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/LOG="'+$uninstallLog+'"')) -WindowStyle Hidden -Wait -PassThru
if ($process.ExitCode -ne 0) { throw 'Controlled uninstall failed' }
if (Test-Path -LiteralPath (Join-Path $destination 'Contents\x86_64-win\PHASMORA.vst3')) { throw 'Plugin binary not removed' }
if (Test-Path $registry) { throw 'Uninstall registration not removed' }
if (!(Test-Path -LiteralPath $sibling)) { throw 'Unrelated sibling modified' }
Write-Output 'Controlled reinstall and uninstall passed; unrelated sibling preserved. Not a clean-machine or administrator install test.'

