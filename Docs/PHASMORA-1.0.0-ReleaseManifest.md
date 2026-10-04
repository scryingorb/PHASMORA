# PHASMORA 1.0.0 release manifest

Branding rename verified 2026-10-04, America/Los_Angeles.
Windows x64 VST3 audio effect, stereo input/output, C++17; JUCE 8.0.12;
Visual Studio 2019 v142 (14.29.30133), Windows SDK 10.0.19041.0;
CMake 3.31.6; static /MT Release runtime; Inno Setup 6.7.3.

Public name/manufacturer PHASMORA; version remains 1.0.0.
All identity codes, parameter IDs, DSP/preset/visualizer implementations retained.
See PHASMORA-1.0.0-RenameVerification.md for exact IDs, file inventory,
compatibility/migration tests, scope and outstanding manual release checks.

## Final SHA-256 (relative to dist)

```text
013fc30fb06aaa80f0dd28100427fc0c1548ed58447e3a9ec3b5b3d19174c1df  PHASMORA-1.0.0-Windows/PHASMORA-1.0.0-Windows-Setup.exe
43db82e31c7f268a1ed96fe317926c499f49c0f74046475bc4460363cf68827d  PHASMORA-1.0.0-Windows.zip
5a7ee071eb92da6a1eb665593cbc099f01a0a02c956045d57f71c8984da6009c  PHASMORA-1.0.0-Windows/PHASMORA.vst3/Contents/Resources/moduleinfo.json
1b950674b23bd8df7a548db5b20e104995938887797694ebc6860030daa2ff5a  PHASMORA-1.0.0-Windows/PHASMORA.vst3/Contents/x86_64-win/PHASMORA.vst3
```

The package-local SHA256SUMS.txt hashes every other packaged file; the sibling
manifest includes the ZIP. No old GHOST release artifacts remain in dist.
Historical releases remain outside dist for development compatibility evidence.

## Source records

PHASMORA-RenameBaseline.json records pre-branding Source hashes.
PHASMORA-1.0.0-SourceHashes.json records final source, assets, tests, build,
installer and public documentation inputs, excluding generated artifacts and
this document to avoid hash cycles. Source manifest SHA-256:
`cc01663880cde5babe20684b350500d4eec3021fa5f05610bc60e07a95bebb36`.

Release build and all five CTest suites pass. Baseline audio matches bit for bit
at four sample rates; all eight old-state values restore. Public deployment is
still subject to the manual host/clean-machine checks and licensing decisions
listed in the verification report. Installer/plugin are unsigned.
