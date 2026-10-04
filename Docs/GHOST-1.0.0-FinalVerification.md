# GHOST 1.0.0 Final Verification

Finalized 2026-10-04 (America/Los_Angeles). Public version: GHOST 1.0.0.

## Implementation preservation

No Source/ or Assets/ file was modified. No CMake configuration, DSP formula,
parameter ID/range/default, preset value, UI layout, visualizer or identifier changed.
The production Release build completed up-to-date, without warnings. Both VST3
bundle files match the pre-finalization tested baseline byte-for-byte. The binary
SHA-256 remains 0c3054ad5c1bb18f043b58803063d743d186c04343b6086f9fbe1b5039731a15.
Windows and JUCE metadata still report 1.0.0. Release/NDEBUG and static /MT runtime
remain configured. The immutable implementation manifest has 31 matching entries;
the final source-input manifest has 57 matching entries.

## Final automated run

All **4/4 existing CTest suites passed; zero failures**. Total wall time 566.72 s.

| Suite | Result | Seconds |
| --- | --- | ---: |
| GHOST_DSPTests | Passed | 566.71 |
| GHOST_ProcessorTests | Passed | 3.25 |
| GHOST_UITests (UI, visualizer, presets) | Passed | 9.48 |
| GHOST_ReleaseTests (metadata, tail, benchmark, integrated stress) | Passed | 42.57 |

Full record: ../final-verification.log. No suites were skipped or replaced with
historical results. Package audit additionally verified IDs/version, exact clean
package contents, ZIP entries, public final wording, unchanged implementation and
all internal/external SHA-256 checksums.

## Established integrated stress scenario

The existing test ran 120 audio seconds per rate, 480 seconds total, using the same
maximum settings, populated history, FREEZE/REVERSE transitions, GHOST automation,
preset switches and editor lifetime events. No new stress configuration was added.

| Rate | Peak | dBFS |
| --- | ---: | ---: |
| 44.1 kHz | 0.435160 | -7.22702 |
| 48 kHz | 0.457055 | -6.80064 |
| 88.2 kHz | 0.466500 | -6.62297 |
| 96 kHz | 0.475726 | -6.45286 |

Finite, bounded output; no crash; no invalid-read regression failures; zero detected
C++ new allocations in processBlock. Warmed process private bytes stayed unchanged
at three rates and increased by 28,672 bytes at 48 kHz, within the existing 2 MiB
growth threshold. No memory-growth regression was detected; this is not a proof
against every leak or a sanitizer run. Existing dedicated read-bounds tests passed.
Long finite-tail tests retained endpoint peaks of -81.0253/-75.4237 dBFS.

## Installer and distribution

Final installer: dist/GHOST-1.0.0-Windows/GHOST-1.0.0-Windows-Setup.exe.
Destination and behavior are unchanged: administrator elevation by default;
complete bundle into C:\Program Files\Common Files\VST3\GHOST.vst3; same AppId;
registered reinstall/uninstall; no unrelated plugin deletion rules.

The final installer passed controlled per-user installation, reinstall, complete
bundle hash comparison and uninstall in build/Installer-Test. Its temporary HKCU
registration was removed and an unrelated sibling file survived. This test did not
modify the owner's system-wide plugin installation and is not a clean-machine or
administrator-mode test. See ../final-installer-verification.log.

Clean package top level: GHOST.vst3, GHOST-1.0.0-Windows-Setup.exe, README.md,
LICENSE-PENDING.txt, THIRD-PARTY-NOTICES.txt, ThirdPartyLicenses, SHA256SUMS.txt.
No test executables, build folders, logs, previews, toolchains or internal manifests
are distributed. The final ZIP contents were inspected and match their on-disk files.
Package checksums cover every other package file; dist/SHA256SUMS.txt covers the
installer, ZIP and raw bundle files. No checksum file attempts to hash itself.

## Manual validation recorded

The owner reports the tested implementation's installer launches, GHOST installs,
FL Studio discovers it, the editor opens, and audio processing works. These results
apply to the preserved binary. Manual validation of long-tail rendering, HiDPI,
Smart Disable or every preset has not been reported and is not claimed here.

## File changes

Modified: README.md; Installer/BeforeInstall.txt; LICENSE-PENDING.txt;
THIRD-PARTY-NOTICES.txt; package-release.ps1 (internal package checksum manifest);
Tests/verify_release.py (final integrity/wording/contents checks); .gitignore;
Docs/ReleaseVerification.md (historical label and link to this final report).

Created: this report; GHOST-1.0.0-ReleaseManifest.md;
GHOST-1.0.0-ImplementationBaseline.json; GHOST-1.0.0-SourceHashes.json;
final-build-verification.log, final-package-verification.log,
final-installer-verification.log, final-verification.log.
Regenerated final installer, ZIP and dist/SHA256SUMS.txt; added the package-local
SHA256SUMS.txt. The standalone bundle is an exact copy of the tested binary.

## Git and public distribution

This directory is not a Git repository, so clean/dirty status cannot be reported.
No repository, commit, tag, push or remote release was created. The manifest gives
preparation instructions for an eventual v1.0.0 tag. .gitignore excludes generated
dist, build, toolchains and local root logs; the source manifests should be committed.

Public distribution remains blocked on owner selection of the GHOST license and
confirmation of JUCE/dependency/logo and installer-tool rights. No legal license
was invented. Artifacts remain unsigned. A true clean-machine test and expanded
host checks remain outstanding. FREEZE capture is not serialized and requires
recapture after reopening; host handling of infinite tails varies. These legitimate
limitations are retained in the public README. No final automated code failure remains.
