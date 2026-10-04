# PHASMORA 1.0.0 rename verification

Verified 2026-10-04. Controlled branding rename only. No DSP, preset values,
parameter defaults/ranges/mappings, visualizer behavior, or tail behavior changed.

## Files

Modified: CMakeLists.txt; build.ps1; package-release.ps1; README.md;
LICENSE-PENDING.txt; THIRD-PARTY-NOTICES.txt; Installer/BeforeInstall.txt;
Source/PluginProcessor.cpp; Source/PluginEditor.cpp; Source/UI/GhostLookAndFeel.cpp;
Tests/GhostPresetTests.cpp; Tests/GhostReleaseTests.cpp; Tests/GhostUITests.cpp;
Tests/verify_release.py; Tests/VerifyInstaller.ps1.

Created: Assets/PhasmoraLogo.png; Tests/GhostBrandingTests.cpp;
Tests/VerifyMigration.ps1; Tests/Fixtures/legacy-state-0.bin and legacy-state-1.bin;
Tests/Fixtures/audio-{44100,48000,88200,96000}.sha256;
Docs/PHASMORA-RenameBaseline.json; this report;
Docs/PHASMORA-1.0.0-ReleaseManifest.md; Docs/PHASMORA-1.0.0-SourceHashes.json;
verification logs and UI previews listed below; final PHASMORA distribution.

Renamed/archived: Installer/GHOST.iss -> Installer/PHASMORA.iss (also updated);
Assets/GhostLogo.png -> Docs/HistoricalAssets/GhostLogo.png (not embedded);
old dist GHOST folder/ZIP -> build/BrandingBaseline/HistoricalDistribution/.
Public bundle, installer and ZIP are now PHASMORA names. Internal build target
GHOST remains; PHASMORA_VST3 is a convenience alias. No Git repository was present;
file-change inventory is compared against the previous release's source hashes.

## Product, asset and identity

Public product and manufacturer: PHASMORA. Version: 1.0.0.
Visible macro and host parameter display name: PHASMORA. Parameter ID: ghost.
Original supplied logo copied without editing to Assets/PhasmoraLogo.png.
SHA-256: edbab04f665854cc044e4121fef7798f79885b4f2bee7cb5d1bf518342d8c3b4.
The original aspect ratio and existing 820 x 540 layout/placement are retained.

Preserved for automation, saved state, host identity and upgrade registration:

- Manufacturer code Ghst / 0x47687374; plugin code Gh01 / 0x47683031.
- Processor CID ABCDEF019182FAEB4768737447683031.
- Controller CID ABCDEF011234ABCD4768737447683031.
- Bundle ID com.ghost.audio.ghost; state root GHOST_STATE.
- Installer AppId {E14C74E3-461B-48F2-AC84-D6F12F08D9F0}.
- Parameter IDs ghost, memory, decay, blur, drift, mix, reverse, freeze.
- Internal Ghost* classes, test names and build identifiers.

## Verification

Release VST3 and test builds succeeded; rename-build-verification.log contains no
new compiler warnings. Full CTest: 5/5 passed, zero failures, 331.23 seconds.
Suites: GHOST_DSPTests, GHOST_ProcessorTests, GHOST_UITests, GHOST_ReleaseTests,
GHOST_BrandingTests. UI suite includes preset and visualizer checks.
Complete log: phasmora-rename-verification.log.

Before branding edits, the old processor captured two serialized states (FREEZE
off/on) and 16 seconds of stereo output at each of 44100, 48000, 88200 and 96000 Hz.
After branding, all eight parameter values restored correctly and all compared
float samples were bit-identical. The scenario exercises state restoration,
automation, reverse, freeze and preset switching. SHA-256 fixtures also passed in
a fresh working directory without the large baseline recordings. This verifies
the tested scenarios, not every possible input or a live DAW substitution.

Tests/verify_release.py passes: DSP/preset/visualizer files match pre-rename hashes
byte for byte; the three changed implementation files contain only intended
branding substitutions (allowing CRLF/LF normalization). Metadata CIDs, parameter
IDs, state root, bundle contents, ZIP contents and every packaged checksum pass.

Representative previews under build/UI-Previews-PHASMORA/:
PHASMORA-default.png, PHASMORA-active.png, PHASMORA-active-2x.png,
PHASMORA-freeze.png, PHASMORA-reverse.png, PHASMORA-presets.png.
Reviewed 1x/2x rendering: original logo fits without clipping, full PHASMORA macro
label is readable, preset selector fits, existing aesthetic is retained.

## Installer migration

Default destination: C:/Program Files/Common Files/VST3/PHASMORA.vst3.
Migration recognizes the sibling GHOST.vst3 only by known old binary SHA-256,
legacy CIDs, an explicit allowed file/directory list, and no reparse points.
Known old binary SHA-256:
0c3054ad5c1bb18f043b58803063d743d186c04343b6086f9fbe1b5039731a15.
Recognized old bundle is staged under a non-VST3 backup name, replaced by PHASMORA,
and removed after successful installation. Failed installation restores the old
bundle. Unknown/modified bundles or unexpected user files cause refusal without
deletion. Other old builds require manual review; no unrelated plugins are removed.

Controlled current-user workspace tests passed: install/reinstall/uninstall,
unknown old bundle refusal, unexpected file protection, forced copy failure with
rollback, actual known legacy installer migration, no duplicate old bundle,
registry cleanup and unrelated sibling protection. Logs:
phasmora-installer-verification.log and phasmora-migration-verification.log.
These are not an elevated system-wide clean-machine or FL Studio rescan test.

## Artifacts and remaining release work

Build: build/GHOST_artefacts/Release/VST3/PHASMORA.vst3.
Distribution: dist/PHASMORA-1.0.0-Windows/.
Bundle: dist/PHASMORA-1.0.0-Windows/PHASMORA.vst3.
Installer: dist/PHASMORA-1.0.0-Windows/PHASMORA-1.0.0-Windows-Setup.exe.
ZIP: dist/PHASMORA-1.0.0-Windows.zip.
Checksums: dist/SHA256SUMS.txt and dist/PHASMORA-1.0.0-Windows/SHA256SUMS.txt.

Remaining Ghost text is intentional: Ghost Guitar preset, descriptive terminology,
legacy migration messages, compatibility documentation and internal/historical
names. Current product UI and metadata say PHASMORA. A host's cached old name may
require a rescan. Search evidence: build/branding-search-audit.txt.

The owner's prior FL Studio manual validation applies to GHOST, not PHASMORA.
Old FL Studio project substitution after migration/rescan still needs manual
verification. Also pending: real elevated clean-machine install/uninstall, host
HiDPI/Smart Disable checks as applicable, final distribution licensing selection
(including JUCE/tool obligations), and signing if desired. Artifacts are unsigned;
LICENSE-PENDING.txt remains. No claim of public licensing clearance is made.
