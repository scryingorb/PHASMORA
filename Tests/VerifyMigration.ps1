$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$testRoot=[IO.Path]::GetFullPath((Join-Path $projectRoot 'build\Migration-Test'))
$allowed=[IO.Path]::GetFullPath((Join-Path $projectRoot 'build'))+[IO.Path]::DirectorySeparatorChar
if (!$testRoot.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe test target' }
$registry='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{E14C74E3-461B-48F2-AC84-D6F12F08D9F0}_is1'
if (Test-Path $registry) { throw 'Do not overwrite an existing per-user installation' }
$legacy=Join-Path $testRoot 'GHOST.vst3'
$new=Join-Path $testRoot 'PHASMORA.vst3'
$setup=Join-Path $projectRoot 'dist\PHASMORA-1.0.0-Windows\PHASMORA-1.0.0-Windows-Setup.exe'
$oldSetup=Join-Path $projectRoot 'build\BrandingBaseline\Legacy-Setup.exe'
New-Item -ItemType Directory -Path $legacy -Force | Out-Null
$sentinel=Join-Path $legacy 'unrelated.keep'
Set-Content -LiteralPath $sentinel -Value 'unrelated plugin; must survive'
function Install-Controlled($exe,$destination,$logName) {
    $logPath=Join-Path $testRoot $logName
    $argsList=@('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/CURRENTUSER',('/DIR="'+$destination+'"'),('/LOG="'+$logPath+'"'))
    $p=Start-Process -FilePath $exe -ArgumentList $argsList -WindowStyle Hidden -Wait -PassThru
    return $p.ExitCode
}
$code=Install-Controlled $setup $new 'unrecognized.log'
if ($code -eq 0 -or !(Test-Path -LiteralPath $sentinel)) { throw 'Unrelated legacy folder was not protected' }
Write-Output 'PASS: unrecognized GHOST.vst3 refused and unrelated file preserved.'
Remove-Item -LiteralPath $sentinel
Remove-Item -LiteralPath $legacy
if ((Install-Controlled $oldSetup $legacy 'legacy-install.log') -ne 0) { throw 'Legacy installation failed' }
Set-Content -LiteralPath $sentinel -Value 'unexpected user file; must survive'
$code=Install-Controlled $setup $new 'unexpected-file.log'
if ($code -eq 0 -or !(Test-Path -LiteralPath $sentinel)) { throw 'Unexpected user file was not protected' }
Write-Output 'PASS: recognized legacy with unexpected user content refused without deletion.'
Remove-Item -LiteralPath $sentinel
# Force an install-time copy failure after staging the old bundle; verify rollback.
New-Item -ItemType Directory -Path $new -Force | Out-Null
$obstruction=Join-Path $new 'Contents'
Set-Content -LiteralPath $obstruction -Value 'controlled copy obstruction'
$code=Install-Controlled $setup $new 'rollback.log'
if ($code -eq 0 -or !(Test-Path -LiteralPath (Join-Path $legacy 'Contents\x86_64-win\GHOST.vst3')) -or (Test-Path -LiteralPath ($legacy+'.phasmora-migration'))) { throw 'Migration rollback did not restore legacy bundle' }
Remove-Item -LiteralPath $obstruction
Write-Output 'PASS: forced install failure restores the staged legacy bundle.'
$sibling=Join-Path $testRoot 'UnrelatedPlugin.keep' 
Set-Content -LiteralPath $sibling -Value 'unrelated sibling'
if ((Install-Controlled $setup $new 'migration.log') -ne 0) { throw 'Migration failed' }
if ((Test-Path -LiteralPath $legacy) -or (Test-Path -LiteralPath ($legacy+'.phasmora-migration'))) { throw 'Legacy duplicate or backup remains' }
$source=Join-Path $projectRoot 'build\GHOST_artefacts\Release\VST3\PHASMORA.vst3'
foreach ($file in Get-ChildItem -LiteralPath $source -Recurse -File) {
    $installed=Join-Path $new $file.FullName.Substring($source.Length+1)
    if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $installed).Hash) { throw 'Migrated bundle mismatch' }
}
Write-Output 'PASS: legacy installation migrated to PHASMORA; exact new bundle; no duplicate GHOST bundle.'
if ((Install-Controlled $setup $new 'reinstall.log') -ne 0) { throw 'Reinstall failed' }
$uninstaller=Join-Path $new 'Uninstall\unins000.exe'
$p=Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -WindowStyle Hidden -Wait -PassThru
if ($p.ExitCode -ne 0 -or (Test-Path $registry) -or (Test-Path -LiteralPath (Join-Path $new 'Contents\x86_64-win\PHASMORA.vst3'))) { throw 'Migration uninstall failed' }
if (!(Test-Path -LiteralPath $sibling)) { throw 'Unrelated sibling removed' }
Write-Output 'PASS: migrated reinstall/uninstall, registry cleanup, unrelated sibling protection.'
