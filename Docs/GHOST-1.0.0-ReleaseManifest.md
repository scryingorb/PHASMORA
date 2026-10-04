# GHOST 1.0.0 Release Manifest

Public version: **GHOST 1.0.0**, Final.
Final build verification and packaging date: **2026-10-04 (America/Los_Angeles)**.
The existing tested Release binary was preserved exactly; the production build was
an up-to-date build, not a new implementation compilation. Final installer and ZIP
were regenerated with final documentation and fresh checksum manifests.

## Build configuration

Windows x64 Release; Visual Studio 16 2019 / MSVC v142 (14.29.30133 tool directory),
Windows SDK 10.0.19041.0; CMake 3.31.6; JUCE 8.0.12; C++17; static /MT runtime;
NDEBUG enabled. Installer: Inno Setup 6.7.3. No plugin source or asset changed during
finalization. The build log contains no warnings. CMake configuration and build
scripts are included in the source hash manifest below. Dependency versions and
source hashes aid reproduction; byte-identical rebuilds on other toolchains are
not promised.

## Stable identity

- Product/manufacturer: GHOST / GHOST
- Manufacturer code: Ghst (0x47687374)
- Plugin code: Gh01 (0x47683031)
- Bundle ID: com.ghost.audio.ghost
- VST3 processor CID: ABCDEF019182FAEB4768737447683031
- VST3 controller CID: ABCDEF011234ABCD4768737447683031
- VST3 class: Fx, stereo input/output
- Installer AppId: {E14C74E3-461B-48F2-AC84-D6F12F08D9F0}
- Parameter IDs, in original order: ghost, memory, decay, blur, drift, mix, reverse, freeze
- Parameter version hints remain 1; ranges/defaults/mappings unchanged from tested behavior.

Defaults remain 65/40/40/20/10/35 percent and REVERSE/FREEZE off. Factory values,
DSP, tail reporting, UI and visualizer remain frozen at the tested implementation.

## Artifact hashes

Paths are relative to dist. SHA-256:

```text
61a18cd6576ce7267bb055ca1aefb9ebebb5ef8d8cfea7e75bce161a96763701  GHOST-1.0.0-Windows/GHOST-1.0.0-Windows-Setup.exe
4f6a328c199c08cc8009f1d174417965f785c2b2b3fd3cc1177c040971eceb03  GHOST-1.0.0-Windows.zip
9b5e918a2f504a0456aa7455c8e636155009b2b6f58034e4f01adac65b80dd3d  GHOST-1.0.0-Windows/GHOST.vst3/Contents/Resources/moduleinfo.json
0c3054ad5c1bb18f043b58803063d743d186c04343b6086f9fbe1b5039731a15  GHOST-1.0.0-Windows/GHOST.vst3/Contents/x86_64-win/GHOST.vst3
```

The package contains its own SHA256SUMS.txt covering every other package file.
The sibling dist/SHA256SUMS.txt includes the ZIP hash; the ZIP cannot include its
own hash without a circular dependency. The optional raw VST3 bundle is supplied
as a directory; each of its two files is hashed individually.

## Source baselines

- `GHOST-1.0.0-ImplementationBaseline.json`: pre-finalization Source/Assets and tested
  VST3 bundle hashes; all still match exactly.
- `GHOST-1.0.0-SourceHashes.json`: 57 source, asset, test, build, installer, public
  documentation and licensing inputs after finalization. Generated distributions
  and these manifest documents are excluded to avoid hash cycles.

SHA-256 of implementation baseline: `42a538edd62bead1a1e2a40c04e2deac51df4b215546273ca782fb8fce3ce066`.

SHA-256 of final source manifest: `67c369642b382b0e2a4e84bc1cb3891e4291d45f633a488b2459d4670958b0ea`.

Final automated/manual scope and limitations: see `GHOST-1.0.0-FinalVerification.md`.
The README records the owner's confirmed installer/install/FL Studio discovery,
opening and audio-processing validation. Further host-specific tests are not implied.

## Source-control tag preparation

This workspace has no .git directory and git status reports that it is not a Git
repository. A clean/dirty working-tree result is therefore unavailable. No repository,
commit, tag, push, or remote release was created. The intended tag is `v1.0.0`.

Before tagging, initialize or import the intended source repository, review and
commit the audited source/build/docs/test files and final manifests, verify hashes,
then create an annotated v1.0.0 tag on that commit. Do not include build, .tools, dist,
or local logs: .gitignore now excludes generated distributions and root verification
logs as well as existing build/toolchain directories. Keep release artifacts separately
with this manifest. No remote publication is authorized by these preparation notes.

## Distribution status

Software/package finalization is complete subject to the verification report.
Public distribution remains blocked on owner selection of GHOST's license and
confirmation of JUCE/dependency/logo and installer-tool rights. Artifacts remain
unsigned; true clean-machine and expanded host checks are outstanding. The manifest
is an integrity record, not a software license or distribution-rights grant.
