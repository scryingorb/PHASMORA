# Historical: GHOST 1.0.0 Release Candidate 1 verification

Final status and owner-reported FL Studio validation: see GHOST-1.0.0-FinalVerification.md.

This is the release-engineering record. The shipped README is the product guide.
No manual DAW or clean-machine result is claimed.

## Changes and identity

Production version is 1.0.0 in CMake, JUCE metadata, generated Windows resources,
version label, installer and distribution filenames. CMake now uses the static
MSVC runtime for a self-contained x64 Windows binary. A missing incremental
resource dependency was fixed so version changes regenerate Windows file metadata.

Identifiers preserved:
- Manufacturer: GHOST; four-character code Ghst, hex 0x47687374.
- Product: GHOST; plugin code Gh01, hex 0x47683031.
- Bundle ID: com.ghost.audio.ghost.
- VST3 processor CID: ABCDEF019182FAEB4768737447683031.
- VST3 controller CID: ABCDEF011234ABCD4768737447683031.
- VST3 category Fx; stereo in/out; no MIDI; eight unchanged parameter IDs/version hints.

Only GHOST's APVTS startup default changed, 35 to 65 percent. All other defaults
remain MEMORY 40, DECAY 40, BLUR 20, DRIFT 10, MIX 35, REVERSE/FREEZE off. The squared
macro amplitude changes from 0.1225 to 0.4225 (+10.75 dB), providing a more apparent
starting effect without high feedback or an enabled special mode. Existing projects
restore their saved values. No preset auto-load was introduced. This justification
is based on the mapping and tests, not a completed musical listening session.
The README contains the final parameter table and all ten unchanged preset values.

## Exact tail metadata

Let g = clamp(GHOST / 100, 0, 1), d = clamp(DECAY / 100, 0, 1) * g^2.
Invalid float values use the conservative normalized fallback 1.
Let q = min(0.95, 1.08 * 0.95 * d * (2-d) * g^2).

- FREEZE ON: return positive infinity.
- FREEZE OFF and q = 0: return 12 seconds.
- Otherwise: clamp(12 + ceil(5 * ln(0.001) / ln(q)), 12, 686) seconds.

The 12-second allowance covers stored history plus short read/diffusion transitions.
The 5-second effective loop age gives margin above the larger full-memory channel's
weighted mean delay (4.6123 s). The factor 1.08 allows for changing stereo gain sums.
This is an engineering estimate to a practical -60 dB region, not an exact silence
bound for every input/automation history. It ignores current MEMORY to avoid
underreporting already-populated deeper history. Startup reports 30 seconds; zero
DECAY reports 12; maximum GHOST/DECAY reports 686. MIX does not erase stored tails.

JUCE 8.0.12's VST3 getTailSamples explicitly maps std::isinf to kInfiniteTail.
Positive infinity is intentional metadata, never a generated audio sample.
Parameter and restored-state changes are visible on the next host query. This JUCE
wrapper has no dedicated tail-change notification path; no false latency-change
notification or audio-thread host restart was added. Hosts may cache tail values.

At 48 kHz, a 10-second 0.5 DC excitation followed by silence was tested through
697 seconds total. The final observation window starts at 686 seconds (676 seconds
after input ends, conservatively earlier than the reported horizon). Its peaks:
- Forward, clean: 0.0000888658 (-81.0253 dBFS).
- All-max DRIFT/BLUR with REVERSE: 0.000169362 (-75.4237 dBFS).

No DSP tail is shortened: reporting changes no samples or engine state. The unused
1400-second mathematical-bound constant remains annotated as historical DSP
analysis, not used by the host. For held FREEZE exports, choose an explicit endpoint
or release FREEZE before ending. FL Studio render-tail caching remains unverified.

References: bundled juce_audio_plugin_client_VST3.cpp getTailSamples;
https://steinbergmedia.github.io/vst3_doc/vstinterfaces/classSteinberg_1_1Vst_1_1IAudioProcessor.html
and the FL Studio wrapper/export manual links in README.

## Real-time and numerical audit

The complete processor callback uses cached parameter atomics, fixed scalar setters,
read-only peak scans and GhostEngine processing. Inspection found no callback heap
allocation, vector resizing, mutex, filesystem, logging, UI access, blocking or
random-device use. Ring/diffusion allocation happens in prepare. FREEZE uses fixed
copy-on-write storage and epoch tags, not a ten-second copy at activation. REVERSE
uses fixed region heads reading the same buffers. Presets execute host-notified
parameter changes on the message thread and never reset the engine.

JUCE APVTS copyState/replaceState and XML serialization allocate and lock internally.
They remain outside GHOST's processBlock and must not be invoked as real-time audio
operations. State restoration publishes existing parameter values; it does not
serialize audio history. This is a limitation of those host-state operations, not
a claim that every plugin entry point is real-time safe.

ScopedNoDenormals remains around engine processing; meters flush tiny values.
Input rejects non-finite or absurd >8 absolute samples; feedback guards non-finite
state and uses normalized tanh return with maximum coefficient 0.95. Fractional
reads retain finite/bounds guards; reverse validates recorded regions. One-pole
cutoffs remain stable and all-pass |g|=0.5. Existing fault, bounds, automation and
long stress tests pass. No new output limiter or safety maximum was introduced.

The release test intercepts C++ new/new[] while processBlock executes and detects
zero allocations. This does not intercept every possible C malloc/system allocator;
source inspection supplements it. Private process memory after warmup stayed flat
or decreased in the final integrated run. No full sanitizer/proof of leak absence
or all possible host scheduling behavior is claimed.

## Memory

Let N = ceil(10 * sampleRate) + 1 stereo-history frames. Persistent sample storage:
- Live history: 8*N bytes (two float channels).
- Captured immutable values: 8*N bytes.
- Capture epoch tags: 16*N bytes (two uint64 channels).
- FREEZE working history: 8*N bytes.
- Diffusion: 4 * sum(round(rate * t / 1000)) bytes for six times
  3.7, 11.3, 23.9, 4.3, 13.7, 19.1 ms.
- Four live and four frozen voices, filters, cursors and smoothers: small fixed
  scalar overhead; vector headers/cursors/allocator metadata are also additional.

| Rate | Live bytes | Capture + tags + working bytes | Diffusion bytes | Major DSP buffers MiB |
| --- | ---: | ---: | ---: | ---: |
| 44.1 kHz | 3,528,008 | 14,112,032 | 13,404 | 16.836 |
| 48 kHz | 3,840,008 | 15,360,032 | 14,592 | 18.325 |
| 96 kHz | 7,680,008 | 30,720,032 | 29,184 | 36.649 |

All FREEZE capacity is reserved even when off. These are steady prepared DSP
working buffers, not executable size or a whole-process total. Preparation/reprepare
can temporarily hold old/new vectors. An open editor adds about 5.07 MiB for its
1640x1080 RGB background, about 0.35 MiB for its scaled logo, and potentially about
6.05 MiB for JUCE's cached original 1984x800 ARGB image; font/renderer/framework
caches and small paths are extra. Approximate major buffers with these images:
28.3/29.8/48.1 MiB at 44.1/48/96 kHz. Shared cache lifetime affects actual usage.

## CPU sanity

Windows x64 Release, static runtime; Intel64 Family 6 Model 183 Stepping 1. Wall-clock
callback timing only, not FL Studio CPU. Each case pre-fills 11 seconds of history,
then measures five audio seconds at 48 kHz. Rendering is excluded. The measurement
includes inexpensive allocation-counter checks. OS scheduling adds outliers.

| Case | 64 samples, total ms / mean us | 256 samples, total ms / mean us |
| --- | ---: | ---: |
| Defaults | 66.833 / 17.822 | 66.344 / 70.805 |
| Maximum normal | 68.451 / 18.254 | 62.070 / 66.244 |
| REVERSE ON | 63.505 / 16.935 | 62.929 / 67.160 |
| FREEZE ON, forward | 236.544 / 63.079 | 235.377 / 251.202 |
| All maximum, REVERSE + FREEZE | 234.413 / 62.510 | 236.399 / 252.293 |

Worst observed block was 1409.4 us in 48k/64 maximum-normal (one scheduling outlier,
slightly above the 1333 us block budget). This offline run is not a glitch-free
real-time guarantee. Other worst-case timings are retained in the benchmark log.

## Automated verification

Complete CTest run: 4/4 passed, no failures, total 318.93 seconds.
- DSP: 318.92 s; all existing four-rate tests.
- Processor/state: 3.32 s.
- UI/presets/visualizer: 11.26 s; metering bit-identity at four rates/five block sizes.
- Release: 41.08 s initially; after extending benchmark history warmup, the updated
  release-only rerun passed in 44.47 s. No DSP implementation change occurred.

Integrated RC scenario ran 120 audio seconds at each sample rate. It populated
history and cycled all-max controls, FREEZE, REVERSE, macro zero/full, factory presets
and editor creation/destruction. Peaks:
44.1k 0.435160 (-7.227 dBFS); 48k 0.457055 (-6.801 dBFS);
88.2k 0.466500 (-6.623 dBFS); 96k 0.475726 (-6.453 dBFS).
No non-finite output or crashes, no C++ callback allocations, no sustained private
memory increase after warmup. Existing dedicated reverse tests cover read bounds.

Release-source baseline comparison confirms every DSP .cpp, UI source and preset
source unchanged. Two DSP header comments were clarified only. Processor changes
are startup default and tail metadata, not callback processing. Useful inline
read-only diagnostic accessors remain for tests; no production test runner, console
logging, debug image or absolute developer path was bundled. NDEBUG and /MT verified.

UI snapshots in build/UI-Previews-RC1 cover default, active, reverse, freeze, presets,
zero and active 2x. Visual inspection found no clipped controls, logo overlap or
unreadable readout at 1x/2x. Keyboard, tooltip, attachments and timer/lifetime tests
pass. Actual hover feel, OS scaling and live animation in FL Studio remain manual.

## Installer and artifacts

Inno Setup 6.7.3, stable AppId {E14C74E3-461B-48F2-AC84-D6F12F08D9F0}; default requires
administrator elevation and installs the whole bundle at Common Files/VST3/GHOST.vst3.
Only GHOST-owned files are registered for uninstall. /CURRENTUSER and /DIR are
explicit command-line overrides used solely for the controlled verification.

The first sandboxed controlled install could not create its HKCU uninstall key
(access denied) and rolled back. Running the same scoped test outside the sandbox
succeeded: install, reinstall, complete bundle hash comparison, uninstall registration
removal and preservation of an unrelated sibling. System-wide plugin folders were
not modified. This is not an administrator-install or real clean-machine test.

Bundle inspection: only Contents/x86_64-win/GHOST.vst3 and
Contents/Resources/moduleinfo.json. Windows file/product version 1.0.0, expected
CIDs and Fx metadata verified. dumpbin reports only Windows system DLL imports;
no VC redistributable import, debug artifact or developer absolute-path string.
Package/ZIP files and all SHA-256 entries are checked by Tests/verify_release.py.

Outputs: dist/GHOST-1.0.0-Windows, its GHOST-1.0.0-Windows-Setup.exe, the sibling
GHOST-1.0.0-Windows.zip and SHA256SUMS.txt. Raw VST3 bundle file hashes are included
alongside installer/ZIP hashes. ZIP does not recursively contain its own checksum.

## Before public release

Owner must select the GHOST license and confirm JUCE/dependency/logo rights and
installer-tool terms (this Inno compiler identifies non-commercial use only).
Original selected third-party notices are included, without pretending they select
a license for GHOST. Installer and binary are unsigned; signing is recommended.
Complete a real clean-machine administrator install/uninstall and live FL Studio
listening/automation/render-tail/Smart Disable/HiDPI tests. No such manual validation
is claimed. No release-engineering code/test failures remain.
