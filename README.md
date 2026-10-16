# PHASMORA

[Download PHASMORA 1.0.0 for Windows x64](https://github.com/scryingorb/PHASMORA/releases/tag/v1.0.0)

Proprietary freeware. See [LICENSE.txt](LICENSE.txt).

**PHASMORA 1.0.0 | Windows x64 VST3**

PHASMORA is a supernatural audio-memory effect that reaches into recent sound and
transforms it into drifting, blurred, reversed, and frozen spectral textures.

## What PHASMORA Does

A shared ten-second stereo memory feeds four independently positioned voices.
The original dry signal stays separate from the ghost processing. Start with vocals,
clean guitar, keys or percussion, then adjust PHASMORA and MIX to taste.

## Features

- Four ghost voices with independent timing, level and stereo placement
- MEMORY depth, controlled DECAY feedback, DRIFT pitch movement and BLUR diffusion
- REVERSE phrase playback and FREEZE capture
- PHASMORA macro and equal-power dry/wet MIX
- Custom 820 x 540 interface, original embedded logo and animated ectoplasm
- Ten factory presets, host automation and saved parameter state

## Controls

| ID | Display name | Type | Range | Default | Behavior / automation |
| --- | --- | --- | --- | --- | --- |
| `ghost` | PHASMORA | Continuous float | 0-100% | 65% | Overall ghost intensity; 150 ms ramp and existing mapped smoothers |
| `memory` | Memory | Continuous float | 0-100% | 40% | How far into history voices reach; 100 ms read-position crossfades |
| `decay` | Decay | Continuous float | 0-100% | 40% | Ghost regeneration; 100 ms smoothing |
| `blur` | Blur | Continuous float | 0-100% | 20% | Softens and diffuses wet ghosts; 150 ms smoothing |
| `drift` | Drift | Continuous float | 0-100% | 10% | Slow pitch instability; 150 ms smoothing |
| `mix` | Mix | Continuous float | 0-100% | 35% | Dry/wet balance; 50 ms smoothing |
| `reverse` | Reverse | Boolean | OFF/ON (0/1) | OFF | Reverses ghost regions with the existing transition |
| `freeze` | Freeze | Boolean | OFF/ON (0/1) | OFF | Captures current memory; 150 ms handover |

All eight parameters are automatable. Their IDs, ranges and ordering are unchanged.
For 1.0.0, only the startup PHASMORA value changed (35% to 65%): approximately squared
wet amplitude makes 35% unusually restrained. The new value raises macro wet gain
about 10.8 dB while keeping the other settings moderate. This is a measured mapping
review, not a claim of completed live listening validation. Existing saved parameter
values remain authoritative; no named preset is automatically loaded.

## Factory Presets

| Preset | PHASMORA | MEMORY | DECAY | BLUR | DRIFT | MIX | REVERSE | FREEZE | Purpose |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- | --- |
| Whisper | 35 | 25 | 20 | 15 | 12 | 22 | OFF | OFF | Subtle everyday effect |
| Haunted Vocal | 70 | 45 | 48 | 35 | 22 | 38 | OFF | OFF | Strong usable vocal ghosts |
| Lost Memory | 75 | 80 | 60 | 55 | 28 | 45 | OFF | OFF | Long distant fragments |
| Ectoplasm | 85 | 55 | 72 | 70 | 40 | 50 | OFF | OFF | Dense supernatural texture |
| Deep Space | 80 | 90 | 85 | 82 | 35 | 55 | OFF | OFF | Large slow wash |
| Frozen Choir | 78 | 65 | 68 | 72 | 30 | 45 | OFF | OFF | Feed choir/vocals, then engage FREEZE |
| Reverse Spirit | 82 | 60 | 50 | 42 | 25 | 45 | ON | OFF | Backward memories |
| Ghost Guitar | 58 | 38 | 38 | 24 | 16 | 32 | OFF | OFF | Clean/electric guitar |
| Dream State | 72 | 68 | 78 | 88 | 32 | 52 | OFF | OFF | Soft ambient transformation |
| Possessed | 100 | 82 | 88 | 68 | 72 | 62 | ON | OFF | Extreme demonstration |

Numeric preset values are percentages. Select a preset beside the logo or use the
wrapping previous/next buttons. Manual changes show **Custom** unless the values
match another factory configuration. From Custom, next selects Whisper and previous
selects Possessed. Project state saves actual settings, including manual edits.

Every factory preset turns FREEZE off. **Frozen Choir contains no captured audio**:
feed suitable material into PHASMORA, then engage FREEZE. Selecting a preset preserves
live history and uses the existing parameter transitions.

## Installation

Close your DAW and run `PHASMORA-1.0.0-Windows-Setup.exe`. Accept administrator elevation.
The installer copies the complete bundle to:

```text
C:\Program Files\Common Files\VST3\PHASMORA.vst3
```

Alternatively, copy the entire supplied `PHASMORA.vst3` directory there manually.
Do not copy only its internal binary. The installer supports reinstall/upgrade and
uninstall through Windows Installed apps; it removes only its installed PHASMORA files.
Do not move the bundle before using its uninstaller. Windows 10/11 x64 is the target;
a true clean-machine Windows validation remains outstanding. The C++ runtime is
statically linked; no separate VC runtime installation is required by this binary.

### FL Studio

1. Open Options > Manage plugins, then Find installed plugins; verify the standard
   VST3 directory is scanned. Locate PHASMORA as an **effect**, not an instrument.
2. Insert it on a stereo mixer track, play audio, and try Haunted Vocal.
3. Disable the wrapper's Smart Disable for long/frozen textures if live processing
   is suspended unexpectedly. Smart Disable is a host behavior, not a PHASMORA gate.
4. For normal tails, use the export tail/remainder options and audition the result.
   For indefinite FREEZE, set an explicit render endpoint or automate FREEZE off
   before the end. Do not expect an infinite texture to finish automatically.
5. Save/reopen after editing a preset to confirm values restore. Frozen audio itself
   is not saved: feed new material and toggle FREEZE off/on to recapture.

The finite tail estimate is 30 seconds at startup, 12 seconds at zero DECAY, and
up to 686 seconds at maximum regeneration. FREEZE reports VST3's infinite-tail
sentinel through JUCE. Reporting never truncates audio. Hosts may cache the value;
FL Studio's exact caching/render behavior has not been manually validated.

Host reference: [FL Studio wrapper](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/wrapper.htm)
and [export settings](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/fformats_save_export.htm).

## Build From Source

Requirements: Windows x64, Visual Studio 2019 or later with C++ tools and a Windows
SDK, CMake 3.22+, and JUCE 8.0.12. CMake fetches JUCE unless `GHOST_JUCE_PATH` points
to a local checkout. First-time dependency fetching requires network access.

```powershell
cmake -S . -B build -A x64 -DGHOST_BUILD_TESTS=ON
cmake --build build --config Release --target PHASMORA_VST3 GHOST_DSPTests GHOST_ProcessorTests GHOST_UITests GHOST_ReleaseTests GHOST_BrandingTests --parallel 2
ctest --test-dir build -C Release --parallel 2 --output-on-failure
```

`build.ps1 -Configuration Release` is also available for the plugin build.
The resulting bundle is `build/GHOST_artefacts/Release/VST3/PHASMORA.vst3`.
With Inno Setup 6 installed, run `powershell -NoProfile -ExecutionPolicy Bypass -File ./package-release.ps1` to build the installer,
copy the distribution, create `dist/PHASMORA-1.0.0-Windows-x64.zip`, and write
`dist/SHA256SUMS.txt`. The script accepts `-InnoCompiler` for a nondefault location.

## Architecture

`PluginProcessor` owns the APVTS, engine and read-only visual meters. `Source/DSP`
contains modular buffer, voice, feedback, blur, drift-related voice processing,
reverse, freeze and macro implementations. `Source/UI` contains drawing, knobs and
the timer-driven visualizer. `Source/Presets` contains immutable factory settings.
The audio callback does not depend on an open editor or a selected preset label.

## Testing

Automated coverage includes 44.1, 48, 88.2 and 96 kHz; representative block sizes
32/64/256/512/1024; circular reads, interpolation, smoothing, regeneration, numerical
fault guards, reverse/freeze, macro behavior, saved state, presets and UI lifecycle.
Release-specific checks cover metadata, tail reporting, callback allocations,
processing-time measurements, two-minute integrated stress at every sample rate,
and long maximum-tail decay. Screenshots exercise 1x and 2x rendering.

Current release verification is in `Docs/PHASMORA-1.0.0-PublicationVerification.md`.
Previous rename results are in `Docs/PHASMORA-1.0.0-RenameVerification.md`; earlier
stage measurements are archived in `Docs/DevelopmentHistory.md`. Those internal
engineering documents and logs are deliberately excluded from the distribution.

Manual validation reported by the owner confirmed installation, discovery, opening
and audio processing for the previous GHOST-branded binary. The PHASMORA rename
still needs manual FL Studio rescan and old-project substitution verification.
This confirmation does not imply testing of every preset or host render option.

## Known Limitations

- Frozen captured audio is not serialized into project/preset state; recapture
  after reopening.
- Long-tail rendering, Smart Disable, physical HiDPI interaction and an exhaustive
  audition of every preset have not been confirmed manually.
- UI size is fixed. The visualizer targets 30 FPS; message-thread load can lower it.
- Presets are in-plugin settings, not a host program bank or user-preset file system.
- Tail reporting is a practical decay estimate, not proof of exact digital silence.
  FREEZE can sustain indefinitely. Hosts can cache tail metadata.
- The installer/binary are unsigned. A real clean-machine test remains outstanding.
- PHASMORA is proprietary freeware under `LICENSE.txt`. Third-party components
  retain their own terms; dependency licensing has not been independently verified.

## Version

PHASMORA **1.0.0**.


## Compatibility with the previous product name

Public branding is PHASMORA 1.0.0. Internal GhostEngine/GhostVoice/GhostBuffer and
other Ghost* classes, GHOST build/test targets, GHOST_STATE serialization root,
parameter ID `ghost`, codes Ghst/Gh01, and bundle ID com.ghost.audio.ghost deliberately
remain unchanged. Processor CID ABCDEF019182FAEB4768737447683031 and controller CID
ABCDEF011234ABCD4768737447683031 remain identical. The host-visible macro name is
PHASMORA; its parameter identity and sound are unchanged. Ghost Guitar remains a
factory preset name. These legacy names preserve compatibility and engineering stability.

The installer migrates only the recognized, hash-verified GHOST 1.0.0 bundle beside
PHASMORA.vst3. It refuses unknown/modified bundles or unexpected contents, requiring
manual review rather than deleting unrelated material. Close your DAW before upgrading.
Recognized old files are staged out of the scan path during installation; failed
installation attempts restore the old folder where possible. Do not manually leave
both bundles in VST3 scan paths: they intentionally share the same VST3 identity.
Rescan in FL Studio after migration; cached display names may require refreshing.
Legacy state and exact audio regression are automated; full FL Studio project
substitution across the filename change has not been manually verified.
