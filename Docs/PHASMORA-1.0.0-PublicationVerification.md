# PHASMORA 1.0.0 publication verification

Date: 2026-10-16 (America/Los_Angeles).
Repository: https://github.com/scryingorb/PHASMORA
Release: https://github.com/scryingorb/PHASMORA/releases/tag/v1.0.0

## Scope and changes

Added the owner's Proprietary Freeware License 1.0 verbatim as LICENSE.txt,
replaced the pending notice in packaging, supplied manual installation instructions,
and added the release download link to README. Installer upgrades remove the old
product-owned Documentation/LICENSE-PENDING.txt. Packaging now recreates its exact
generated package directory to avoid stale files, produces a manual-only Windows-x64
ZIP, and writes download-friendly SHA256SUMS.txt. Tests verify ZIP completeness and
installed documentation, including upgrade from the previous PHASMORA build.

No Source/, Assets/, CMake plugin identity, DSP, UI, preset, parameter, saved-state
or tail behavior was changed. Source/assets match the previous verified hashes.
Internal Ghost names and GHOST_STATE are intentional. Codes Ghst / Gh01 and bundle
ID com.ghost.audio.ghost are preserved. Processor CID:
ABCDEF019182FAEB4768737447683031; controller CID:
ABCDEF011234ABCD4768737447683031. IDs ghost, memory, decay, blur, drift, mix,
reverse, freeze remain unchanged. Version remains 1.0.0.

## Performed validation

Fresh Windows x64 Release build from the current source with VS2019/MSVC v142,
Windows SDK 10.0.19041.0, CMake 3.31.6 and the local JUCE 8.0.12 checkout.
The moved, cached build was preserved as build-previous; no stale cache was reused.
Build completed without reported compiler warnings/errors. Inno Setup 6.7.3 compiled
the final installer successfully.

All five CTest suites passed: DSP, processor, UI (including presets/visualizer),
release checks, and branding/state/audio regression. Four rates were exercised:
44100, 48000, 88200 and 96000 Hz. The 16-second stereo baseline at each rate remained
bit-identical, and all eight legacy state values restored with FREEZE off/on.
Total Test time (real) = 326.55 sec

Installer checks ran only under build/Installer-Test and build/Migration-Test in
current-user mode, refusing existing per-user registration. Passed: clean isolated
installation, upgrade from the prior PHASMORA installer, repeated installation,
complete bundle and documentation hashes, obsolete notice cleanup, uninstall,
registration cleanup, and unrelated sibling preservation. GHOST checks passed:
unknown bundle refusal, unexpected-file refusal, forced copy failure rollback,
recognized old installer migration, no duplicate bundle, reinstall and uninstall.
No installed production plugins or DAW projects were targeted.

Tests/verify_release.py passed: branding/version and CIDs, original DSP/preset/
visualizer source hashes, complete bundle, exact manual ZIP file set, matching
package documentation and all SHA-256 values. Final executable signatures checked:
installer and VST3 are both NotSigned.

Local evidence logs: publish-build.log, publish-tests.log, publish-installer.log,
publish-migration.log, publish-package.log. These are generated development evidence,
not installed files. The suite summary below is from the current run.

## Final downloadable artifacts

Installer: PHASMORA-1.0.0-Windows-Setup.exe
Manual archive: PHASMORA-1.0.0-Windows-x64.zip
Checksum manifest: SHA256SUMS.txt

The ZIP contains the complete PHASMORA.vst3 directory, README, INSTALL-WINDOWS.txt,
LICENSE.txt, THIRD-PARTY-NOTICES.txt and ThirdPartyLicenses/. It excludes the installer,
source, toolchains, logs and development files. SHA-256:

```text
9586e14ea61be7c3d7cf4e021b6f8865261b228e7187f9ab9ee0fe5880668b2c  PHASMORA-1.0.0-Windows-Setup.exe
35c8a7389154f2330c378da9852eda45dd604b3a04d633bba6ce66f305fc031e  PHASMORA-1.0.0-Windows-x64.zip
```

## Limitations

Installer/plugin are unsigned. Elevated clean-machine installation, actual old
FL Studio project substitution/rescan, physical HiDPI, Smart Disable and exhaustive
preset audition are not claimed. Frozen audio is not serialized, and FREEZE can
sustain indefinitely. Prior owner-reported GHOST host testing is not PHASMORA
substitution testing. The manual ZIP does not execute migration.

The owner supplied the PHASMORA proprietary freeware license and requested proceeding
without further dependency-licensing confirmation. JUCE/tool/dependency licensing
was not independently verified; no clearance claim is made. Third-party notices
remain included. Original historical reports retain their original dates/status;
this report supersedes their packaging/license-selection status for this release.
