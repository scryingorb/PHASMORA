param([string]$InnoCompiler = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe')
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$cmakeText = Get-Content -LiteralPath (Join-Path $projectRoot 'CMakeLists.txt') -Raw
if ($cmakeText -notmatch 'project\(GHOST VERSION ([0-9]+\.[0-9]+\.[0-9]+)') { throw 'Missing production version' }
$version = $Matches[1]
$distRoot = Join-Path $projectRoot 'dist'
$packageName = "PHASMORA-$version-Windows"
$packageRoot = Join-Path $distRoot $packageName
$bundle = Join-Path $projectRoot 'build\GHOST_artefacts\Release\VST3\PHASMORA.vst3'
if (!(Test-Path -LiteralPath (Join-Path $bundle 'Contents\x86_64-win\PHASMORA.vst3'))) { throw 'Build Release VST3 first' }
$binaryVersion = (Get-Item -LiteralPath (Join-Path $bundle 'Contents\x86_64-win\PHASMORA.vst3')).VersionInfo.ProductVersion
if ($binaryVersion -ne $version) { throw "Stale binary version $binaryVersion" }
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
Copy-Item -LiteralPath $bundle -Destination $packageRoot -Recurse -Force
foreach ($name in @('README.md','LICENSE-PENDING.txt','THIRD-PARTY-NOTICES.txt')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $name) -Destination $packageRoot -Force
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'ThirdPartyLicenses') -Destination $packageRoot -Recurse -Force
& $InnoCompiler "/DProductVersion=$version" "/DProjectRoot=$projectRoot" "/DOutputRoot=$packageRoot" (Join-Path $projectRoot 'Installer\PHASMORA.iss')
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed' }
$installer = Join-Path $packageRoot "$packageName-Setup.exe"
$zipPath = Join-Path $distRoot "$packageName.zip"
# Internal checksums cover every package file except the checksum file itself.
# ZIP hash stays outside the ZIP, avoiding a self-referential checksum cycle.
$packageHashes = Get-ChildItem -LiteralPath $packageRoot -File -Recurse |
    Where-Object { $_.FullName -ne (Join-Path $packageRoot 'SHA256SUMS.txt') } |
    Sort-Object FullName | ForEach-Object {
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        $relative = $_.FullName.Substring($packageRoot.Length + 1).Replace('\','/')
        "$hash  $relative"
    }
$packageHashes | Set-Content -LiteralPath (Join-Path $packageRoot 'SHA256SUMS.txt') -Encoding ascii
Compress-Archive -LiteralPath $packageRoot -DestinationPath $zipPath -Force
$hashes = foreach ($path in @($installer,$zipPath)) {
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    $relative = $path.Substring($distRoot.Length + 1).Replace('\','/')
    "$hash  $relative"
}
Get-ChildItem -LiteralPath (Join-Path $packageRoot 'PHASMORA.vst3') -File -Recurse | ForEach-Object {
    $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $relative = $_.FullName.Substring($distRoot.Length + 1).Replace('\','/')
    $hashes += "$hash  $relative"
}
$hashes | Set-Content -LiteralPath (Join-Path $distRoot 'SHA256SUMS.txt') -Encoding ascii
Write-Output "Packaged $packageRoot"
