# GHOST

JUCE/C++ stereo VST3 audio effect foundation. Manufacturer and product: GHOST.
A ten-second stereo history feeds four independent ghost voices. MEMORY controls
how far back they reach; DECAY controls regeneration; DRIFT adds slow independent
pitch movement; BLUR softens and diffuses memories; REVERSE plays remembered
regions backward; FREEZE captures and sustains remembered material; MIX controls
the dry/wet balance. All parameters are exposed for automation and saved with
the host's plugin state. GHOST scales the configured wet intensity without changing
individual parameter values. The custom editor uses the supplied mint-green GHOST
logo, custom rotary controls, a fixed 820 x 540 layout, and a 30 FPS audio-reactive
ghost-energy halo behind the macro.

## Structure

- `Source/PluginProcessor.*`: stereo buses, APVTS parameters, state persistence,
  and forwarding of prepare/reset/process calls to the DSP engine.
- `Source/PluginEditor.*`: fixed custom editor composition, cached backdrop and embedded logo.
- `Source/DSP/GhostEngine.*`: memory writes, four voices, MEMORY mapping, MIX and feedback routing.
- `Source/DSP/GhostBuffer.*`: configurable circular history with fractional reads.
- `Source/DSP/GhostVoice.*`: independent delay, gain, stereo balance, read crossfades and deterministic DRIFT LFO.
- `Source/DSP/GhostFeedback.*`: normalized stereo return, DECAY smoothing and protection.
- `Source/DSP/GhostBlur.*`: per-voice low-pass filters and short stereo diffusion.
- `Source/DSP/GhostReverse.*`: fixed-state stereo reverse-region readers, shared history only.
- `Source/DSP/GhostFreeze.*`: immutable capture replay and seeded working history.
- `Source/DSP/GhostMacro.*`: intensity mappings and sample-by-sample gain/width smoothing.
- `Source/UI/GhostLookAndFeel.*`: centralized GhostTheme palette, fonts, custom dials/buttons/tooltips and background drawing.
- `Source/UI/GhostKnob.*`: reusable attached rotary control with percentage display and standard JUCE interaction.
- `Source/UI/GhostVisualizer.*`: deterministic wisps/particles, parameter response and UI timer.
- `Assets/GhostLogo.png`: unchanged user-supplied wordmark, embedded by GHOST_Assets.
- `CMakeLists.txt`: VST3 target, C++17, JUCE pinned to 8.0.12.
- `build.ps1`: Windows x64 configure/build helper.
- `Tests/GhostDSPTests.cpp`: buffer and engine regression tests.
- `Tests/GhostFreezeTests.cpp`: capture, isolation, transitions, block sizes and frozen stress tests.
- `Tests/GhostMacroTests.cpp`: macro mappings, pan, automation, mute/persistence and stress tests.
- `Tests/GhostProcessorTests.cpp`: actual processor/APVTS restore and automation tests.
- `Tests/GhostUITests.cpp`: editor attachments, automation, keyboard, reopen and preview rendering.
- `Tests/GhostVisualizerTests.cpp`: timer/state lifecycle and bit-identical metering regression, included in GHOST_UITests.

## Build on Windows

Requires Visual Studio 2019 or newer with the C++ desktop build tools and Windows
SDK, Git, and CMake 3.22 or newer. The first configure downloads JUCE from GitHub.
JUCE includes the VST3 SDK needed for this build.

From PowerShell in this directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

The helper uses the workspace-local CMake in `.tools/cmake-package` when present,
otherwise `cmake` from PATH. Equivalent commands with a compatible CMake on PATH:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target GHOST_VST3 --parallel 2
```

To use an existing JUCE checkout, add `-DGHOST_JUCE_PATH=C:/path/to/JUCE` to
the configure command. Generated Visual Studio projects live in `build/`.

The complete plugin bundle is `build/GHOST_artefacts/Release/VST3/GHOST.vst3`.
Its Windows binary is `Contents/x86_64-win/GHOST.vst3` inside that bundle.
Copy the complete bundle to your host's VST3 scan directory (typically
`C:/Program Files/Common Files/VST3`) when ready to load it. The build does not
automatically install it. Debug builds use `Debug` instead of `Release`.

## Parameters

| ID | Range | Default |
| --- | --- | --- |
| ghost | 0–100% | 35% |
| memory | 0–100% | 40% |
| decay | 0–100% | 40% |
| blur | 0–100% | 20% |
| drift | 0–100% | 10% |
| mix | 0–100% | 35% |
| reverse | boolean | false |
| freeze | boolean | false |

Percentage values are stored in the APVTS in the 0–100 range; host automation
uses JUCE's normalized 0–1 mapping. Parameter IDs are stable and versioned.
All eight parameters are active. Buffer capacity stays fixed at ten seconds;
MEMORY changes read depths without reallocating it. GHOST scales intensity internally
without writing back to the other parameter values. Reported processing latency is
zero (the dry path is immediate). DECAY still defaults to the original 40%, so
set it explicitly to 0% to compare against the previous no-feedback build.
DRIFT retains its 10% default (about 0.5 cents); set DRIFT explicitly to 0% for
pre-DRIFT regression comparisons.
BLUR retains its 20% default; set it explicitly to 0% for pre-BLUR comparisons.
REVERSE retains its false default; OFF runs the existing forward path unchanged.
FREEZE also defaults OFF; frozen audio is not stored in project state.
GHOST retains its 35% default, which is now audible. Set GHOST explicitly to 100%
when comparing with any pre-macro stage or following its historical listening tests.

## Circular buffer math

`GhostBuffer::prepare(rate, maxSeconds, channels)` allocates a ring per channel,
with `ceil(rate * maxSeconds) + 1` slots. The extra slot holds the current sample
while preserving the full requested history, including the interpolation
neighbour for a fractional maximum delay. Each channel has its own write cursor;
the caller writes each channel once per frame. Only prepare allocates storage.
Reset clears it to silence and returns cursors to zero.

After writing, the cursor points to the next slot. The latest sample is therefore
at `nextWrite - 1`. To read D samples into the past:

```text
position = nextWrite - 1 - D
if position < 0: position += capacity
a = floor(position)
b = (a + 1) wrapped to the ring
fraction = position - a
output = samples[a] + fraction * (samples[b] - samples[a])
```

Delay zero means the sample just written, and delay one means the preceding
sample. At position 12.25 the result is 75% of slot 12 plus 25% of slot 13.
The neighbour wraps from the last slot to slot zero when necessary. Delays are
clamped to the prepared range; invalid channels/non-finite delays return silence.
Cleared storage supplies silence until enough real history exists.

## Four-voice MEMORY engine

GhostEngine owns exactly four GhostVoice instances in a fixed-size array, plus
one shared GhostBuffer. It writes both original input channels before processing
the voices for each stereo frame. Each voice owns its delay targets, crossfade
state and precomputed left/right gains. No voice owns or duplicates audio history.
Recording, voice processing and feedback continue at MIX 0%. Only the internal
pre-MIX ghost sum feeds the return; final dry/wet output is never fed back.

For normalized MEMORY m = clamp(MEMORY / 100, 0, 1), the nominal maximum depth D
in seconds is the following continuous, piecewise-linear nonlinear knob curve:

```text
D(m) = 0.5 + 4m    for 0 <= m <= 0.25
       10m - 1     for 0.25 < m <= 0.5
       12m - 2     for 0.5 < m <= 1
voice delay seconds = D(m) * r
voice delay samples = voice delay seconds * sampleRate
```

The implementation interpolates the equivalent five knots at 0/25/50/75/100%.
The lower half of the knob spans only 0.5 to 4 seconds; the upper half spans
4 to 10 seconds. The unequal voice ratios avoid regular rhythmic spacing.

| Voice | Depth ratio r | Gain before stereo balance | Pan |
| --- | --- | --- | --- |
| 1 | 0.20 | -12 dB | -0.25 |
| 2 | 0.39 | -15 dB | +0.25 |
| 3 | 0.64 | -18 dB | -0.55 |
| 4 | 0.93 | -21 dB | +0.55 |

Settled delay times (seconds; each voice has one target, not a random range):

| MEMORY | Voice 1 | Voice 2 | Voice 3 | Voice 4 |
| --- | --- | --- | --- | --- |
| 0% | 0.100 | 0.195 | 0.320 | 0.465 |
| 25% | 0.300 | 0.585 | 0.960 | 1.395 |
| 50% | 0.800 | 1.560 | 2.560 | 3.720 |
| 75% | 1.400 | 2.730 | 4.480 | 6.510 |
| 100% | 2.000 | 3.900 | 6.400 | 9.300 |

Every target is inside the ten-second ring. History not yet recorded after
prepare/reset is silence. Fixed settings are deliberately deterministic at this
stage: asymmetrical timing, descending levels and alternating placement provide
the multiple ghosts. No random fragment scheduler is present; DRIFT now adds
the deterministic pitch modulation documented below.

### MEMORY smoothing

Each voice crossfades two stationary-delay read heads over 100 ms, using
`old * (1 - t) + new * t`. Both heads use GhostBuffer's fractional interpolation.
At DRIFT zero the heads advance with incoming audio at normal speed; MEMORY does
not slide the base delay, avoiding Doppler pitch glides on large MEMORY moves.
Nonzero DRIFT adds the same continuous modulation to both taps. Linear fade weights
preserve headroom for correlated history. Differing audio can briefly overlap or
partially cancel during a transition.

New targets arriving during a fade replace a single pending target without
restarting the active fade. After it finishes, the latest target starts the next
100 ms fade. Settling after the final automation update takes at most about
200 ms (plus up to one host block). Reset cancels fades and clears shared history.

### Stereo and levels

For pan p, `angle = (p + 1) * pi / 4`, left gain is `g * cos(angle)` and right gain
is `g * sin(angle)`, where `g = 10^(gainDb/20)`. This is constant-power balance for
dual-mono input. It operates on the original channels separately, so left-only
audio stays left-only and anti-phase stereo is not collapsed into mono.
Dual-mono hits create different left/right proportions for each ghost.

At BLUR 0% and DECAY 0%, summed wet stays below 0.5 full scale per channel for input
bounded by 1.0, including during MEMORY transitions. With feedback enabled, the
return protection bounds stored input by 1.95 and the pre-diffusion sum below
0.891 per channel. BLUR's all-pass stages redistribute energy in time and can
overshoot that instantaneous peak; unity spectral magnitude is not a peak limit.
The long-session and transient tests cover this behaviour. Equal-power mixing
of correlated dry and wet can still exceed
0 dBFS; there is no output limiter. Leave normal headroom on the source.

For smoothed MIX x in [0, 1], each output channel is:

```text
dry * cos(x * pi/2) + combinedGhosts * sin(x * pi/2)
```

MIX 0% is unchanged dry input; MIX 100% contains only the quiet four-voice sum.
The existing 50 ms MIX ramp advances once per stereo frame. Voice gains and pan
are configured in prepare. Audio processing and parameter updates reuse existing
storage with bounded work: no allocation, vector resizing, locks, I/O or logging.

## DECAY feedback stage

GhostFeedback contains two stored wet samples (one frame of return state), two
normalization constants, and a double-precision smoothed coefficient. The path is:

```text
return[c,n] = f[n] * tanh(wet[c,n-1] / S[c])
memoryWrite[c,n] = dry[c,n] + return[c,n]
wet[c,n] = BLUR diffusion(sum of the four low-pass-filtered ghost voices)
output[c,n] = dry[c,n] * cos(MIX*pi/2) + wet[c,n] * sin(MIX*pi/2)
```

The one-sample causal return preserves the previous write-then-read timing for
all initial ghosts. Each dry sample is written exactly once. MIX is outside the
feedback loop. There is no cross-channel feedback or mono summing. MEMORY only
changes the read positions, not the feedback coefficient.

For normalized DECAY d = clamp(DECAY / 100, 0, 1):

```text
target feedback coefficient = 0.95 * d * (2 - d)
```

| DECAY | Coefficient |
| --- | --- |
| 0% | 0 |
| 25% | 0.415625 |
| 50% | 0.7125 |
| 75% | 0.890625 |
| 100% | 0.95 |

The coefficient ramps linearly over **100 ms**, once per stereo frame. Repeated
identical parameter values do not restart the ramp. At settled zero, return is
exactly zero, preserving the original four-voice response. Turning DECAY down
does not erase already recorded ghosts; they play out from history.

### Normalization and stability

S is the sum of the four positive voice gain/pan weights for each channel:

- Left S = approximately 0.456610641 (normalization multiplier 2.190049706).
- Right S = approximately 0.414601409 (normalization multiplier 2.411955142).

There is no additional arbitrary return attenuation. Division by S compensates
the deliberately quiet output voices and normalizes the pre-BLUR sum into a
weighted average of memory samples, rather than four full-strength loops.
Without this normalization even 100% DECAY would have a short effective loop
gain around 0.4. The final normalized loop coefficient is at most 0.95. Linear
interpolation and MEMORY crossfades also use nonnegative weights summing to one.
At BLUR zero this contracts peak magnitude after input stops. Nonzero BLUR adds
stable, fixed-coefficient all-passes with unity spectral magnitude, plus a
non-amplifying low-pass and complementary blend. It does not add steady-state
loop gain, though it can reshape transient peaks. Automated/time-varying cases
are checked with the long stress tests; no new limiter or gain boost is added.

### Protection and numerical behaviour

`tanh` is applied only to normalized feedback, before multiplying by the DECAY
coefficient. Its small-signal transfer is approximately `x - x^3/3`, with unity
slope near zero and smooth asymptotes at +/-1. Its derivative is never greater
than one, preserving the stability bound. It prevents excessive accumulation
under sustained input (a linear 0.95 loop could otherwise build toward 20 times
input amplitude). Quiet returns are nearly linear; initial voices and normal
dry output are not saturated. Louder regenerated material is gently compressed.

The callback retains JUCE denormal protection. Non-finite parameter updates are
ignored. Non-finite input samples and input magnitudes above 8 (+18.06 dBFS) are
replaced with silence before either the dry output or history; this is an
exceptional upstream-fault guard, not the stability mechanism. Non-finite wet
state is also rejected before capture. Normal finite input is unchanged at MIX
0%. Reset/re-prepare clear both history and return samples, and settle smoothing
to its target. The BLUR filters/diffusers also reset. No later effects are present.

### Tail reporting and limitations

The processor reports a conservative **1400-second maximum tail** to hosts. At
most ten seconds per traversal, 140 traversals with gain 0.95 yield more than
60 dB relative attenuation. This is a worst-case bound, not an audible duration
promise: real tails are much shorter and depend on MEMORY, content and DECAY.
Hosts honoring the full declared tail may produce very long automatic renders;
use an explicit render end for listening tests. Saturation colours hot feedback,
and the plugin is not an output limiter. Setting DECAY to zero stops regeneration
after its 100 ms ramp but does not instantly clear the memory.

## DRIFT pitch movement

Each GhostVoice owns a double-precision phase, phase increment, LFO rate, initial
phase, smoothed depth in samples, and the latest fractional read delays. No
oscillators or storage are constructed in processing. Each stereo frame uses one
sine evaluation and advances the phase once. Phase wraps in [0,1); the sine is
continuous across the wrap. Phase keeps running at zero DRIFT and is never reset
by MEMORY or DRIFT changes. Prepare/reset restore the initial deterministic phase.

| Voice | LFO rate | Initial phase (cycles / degrees) | Maximum delay excursion |
| --- | --- | --- | --- |
| 1 | 0.07 Hz | 0 / 0 degrees | +/-64.726 ms |
| 2 | 0.11 Hz | 0.23 / 82.8 degrees | +/-41.189 ms |
| 3 | 0.17 Hz | 0.51 / 183.6 degrees | +/-26.652 ms |
| 4 | 0.23 Hz | 0.79 / 284.4 degrees | +/-19.699 ms |

For normalized DRIFT d = clamp(DRIFT/100, 0, 1):

```text
C = 50 * d^2                         (nominal cents depth)
q = 1 - 2^(-C/1200)                  (maximum playback-speed deviation)
A = sampleRate * q / (2*pi*lfoHz)     (delay excursion in samples)
readDelay = baseDelay + smoothed(A) * sin(2*pi*phase)
```

This gives 0, 0.5, 3.125, 12.5, 28.125 and 50 cents at DRIFT 0, 10, 25, 50,
75 and 100%. The read timestamp is `time - delay(time)`, so its speed is
`1 - derivative(delay)`. At steady depth, the sine's maximum delay slope is q.
The low-pitch extreme is exactly -C cents; the high extreme is
`1200 * log2(1 + q)`, approximately +48.596 cents at maximum. This small
asymmetry is intrinsic to sinusoidal delay modulation, not a tuning error.

Depth A uses a **150 ms linear ramp** in double precision. Only a changed
parameter target recomputes the cents conversion; no exponentials are evaluated
per audio sample. The same smoothed depth and sine apply to both channels and
both MEMORY crossfade heads. There is no extra oscillator during a crossfade and
no cross-channel audio summing. Base MEMORY targets are unchanged.

Each head limits excursion to `min(baseDelay, historyLength - baseDelay)` before
applying the sine, preventing future/out-of-history reads without clipping the
LFO waveform. All standard GHOST MEMORY settings can use full requested depth:
the shortest 100 ms delay has room for its 64.726 ms excursion, and the longest
9.3 s delay also has ample space. Reads retain fractional interpolation.

DRIFT naturally enters the existing DECAY loop because the modulated voices feed
the pre-MIX wet sum. No extra modulation or gain is added to GhostFeedback.
Interpolation remains a weighted average. See the BLUR section for the new
diffuser's effect on transient peaks. The 1400-second host-tail declaration is unchanged.

Limitations: these are Doppler pitch movements, not a constant pitch shifter or
tempo-synchronized modulation. Large/fast DRIFT automation can temporarily exceed
the steady-state cents range while the delay excursion ramps, though read
positions remain continuous and bounded. MEMORY fades still briefly overlap two
audio positions. Linear interpolation can soften high frequencies on repeated
passes. Determinism means the same state after prepare/reset and the same number
of processed samples; LFO phase is not synced to the DAW timeline or serialized.

## BLUR filtering and diffusion

The complete wet path is:

```text
GhostBuffer -> four MEMORY/DRIFT voices -> one stereo low-pass per voice
            -> summed wet field -> three all-pass stages per channel
            -> blend undiffused/diffused field
            -> wet output through MIX AND existing normalized DECAY return
```

The dry path is untouched. All voices retain their original MEMORY targets,
gain/pan and DRIFT settings. There is no additional pitch spread or long delay.
Filters are per voice; diffusion is shared after summing, for six delay lines
total rather than multiplying diffusion cost by four voices.

### Exact mappings

For b = clamp(BLUR/100, 0, 1), target cutoff in Hz is:

```text
fc = min(20000 * 10^(-b), 0.45 * sampleRate)
voiceFilterBlend = min(1, 4*b)
diffusionBlend = b^2
```

| BLUR | Nominal cutoff | Filter blend | Diffusion blend |
| --- | --- | --- | --- |
| 0% | Bypassed (20 kHz nominal) | 0 | 0 |
| 25% | 11246.8 Hz | 1 | 0.0625 |
| 50% | 6324.6 Hz | 1 | 0.25 |
| 75% | 3556.6 Hz | 1 | 0.5625 |
| 100% | 2000 Hz | 1 | 1 |

Each channel of each voice uses `y = (1-p)*x + p*yPrevious`. The digital pole
is calculated for an exact -3 dB cutoff, rather than an analog approximation:

```text
w = 2*pi*fc/sampleRate
a = 1 - cos(w)
p = 1 + a - sqrt(a*(a+2))
filteredVoice = x + voiceFilterBlend*(y-x)
```

The first quarter of the knob crossfades out of the exact bypass. Below 25% the
blend has a gentler response than the low-pass alone; the table gives its internal
cutoff. From 25% onward the filter is fully applied. One pole preserves more
upper-frequency identity than a steep multi-pole low-pass.

### Short diffusion network

Three serial Schroeder all-pass stages per channel use fixed coefficient g=0.5:

```text
y = delayedState - g*x
newState = x + g*y
H(z) = (z^-N - g) / (1 - g*z^-N)
```

| Channel | Stage 1 | Stage 2 | Stage 3 |
| --- | --- | --- | --- |
| Left | 3.7 ms | 11.3 ms | 23.9 ms |
| Right | 4.3 ms | 13.7 ms | 19.1 ms |

Each length is rounded to the nearest whole sample during prepare. The sum of
storage times is 38.9 ms left / 37.1 ms right, not hundreds of milliseconds.
Internal repeats decay quickly because g is only 0.5. Unequal times create mild
stereo decorrelation without crossfeeding either channel. Output is
`undiffused + diffusionBlend*(diffused-undiffused)`. Complementary linear weights
avoid boosting correlated inputs. At full BLUR the pure all-pass chain changes
phase/time structure without imposing spectral magnitude notches; partial blends
can colour the spectrum through interference, but delay lengths never sweep.

### Automation, feedback and cost

Pole coefficient, filter blend, and diffusion blend each use **150 ms linear
smoothing**, advanced once per stereo frame. Coefficients stay inside the stable
range. Transcendentals are calculated only on a new target/prepare, not per sample.
All-pass coefficient and delay lengths are fixed. Filter/diffuser states stay
warm at BLUR zero, while their output is bypassed exactly. Moving off zero does
not switch in stale state; reset/re-prepare clear every filter and delay.

BLUR happens before both output and DECAY capture. Regenerated memories therefore
soften repeatedly, while unity DC gain retains low-frequency persistence. DECAY's
mapping, normalization, tanh protection, and maximum 0.95 coefficient are unchanged.
DRIFT modulation is upstream and unmodified. Host-tail reporting stays at 1400 s;
its earlier estimate is retained as requested, not recalculated for diffusion.

Added processing: eight one-pole channel filters (four stereo voices), six short
all-pass stages, three smoothers and linear blends. Six buffers are allocated in
prepare, approximately 28.5 KiB total at 96 kHz. The callback allocates nothing,
does no I/O or logging, and uses no locks. No host CPU percentage was benchmarked;
automated test runtime is not a DAW CPU measurement.

Known limitations: all-pass diffusion can reshape a burst into a higher short
peak even though total energy is not increased. Intermediate blends can colour
the spectrum, and mono summing decorrelated channels changes tone. Repeated
filtering intentionally darkens high-DECAY tails. No output limiter or loudness
compensation is added. Subjective metallic ringing/phrase intelligibility and
recorded drum, clap, spoken/sustained vocal and synth listening still need the
DAW checks below; automated tests use impulses, tones and deterministic noise.

## REVERSE remembered-region playback

Each GhostVoice owns one GhostReverse reader containing two fixed-size region
descriptors (outgoing and incoming), plus a separate smoothed mode blend. A
descriptor holds a fractional start delay, source MEMORY target, integer length,
and age counter. No reverse audio is copied or allocated: both stereo channels
read the original shared GhostBuffer. Incoming host blocks are never reversed.

### Region lengths and safe placement

Let D be the existing nonlinear MEMORY depth in seconds (0.5/1.5/4/7/10 at
0/25/50/75/100%). Let `u = clamp((D - 0.5) / 9.5, 0, 1)`. Region lengths are:

```text
Voice 1: L = 0.15 + 0.35*u seconds
Voice 2: L = 0.25 + 0.55*u seconds
Voice 3: L = 0.40 + 0.80*u seconds
Voice 4: L = 0.60 + 1.20*u seconds
```

Lengths are rounded to the nearest sample; a minimum of `2*F+1` samples is
enforced, where `F = round(sampleRate*0.05)` is the region crossfade length. All
normal settings already exceed that minimum.

| MEMORY | Voice 1 | Voice 2 | Voice 3 | Voice 4 |
| --- | --- | --- | --- | --- |
| 0% | 150 ms | 250 ms | 400 ms | 600 ms |
| 25% | 187 ms | 308 ms | 484 ms | 726 ms |
| 50% | 279 ms | 453 ms | 695 ms | 1042 ms |
| 75% | 389 ms | 626 ms | 947 ms | 1421 ms |
| 100% | 500 ms | 800 ms | 1200 ms | 1800 ms |

In sample units, let B be the original voice MEMORY delay, N the region length,
H the buffer's maximum delay (10*sampleRate), and A the maximum possible DRIFT
excursion for that voice (reserved even at DRIFT zero). The newest edge is:

```text
S = clamp(B, A + 2, H - 2*N - A - 2)
```

This reservation is essential: the write cursor keeps advancing while reverse
playback walks backward. Its relative delay grows by TWO samples per output
sample. At MEMORY 100%, voice 4's nominal 9.3-second anchor cannot sustain a
1.8-second region inside a live ten-second ring; S is therefore about 6.3803 s.
Its source region initially covers approximately 6.38-8.18 seconds in the past,
and remains resident throughout playback. Other voices keep their nominal
anchors at the documented five MEMORY settings. This is a capacity constraint,
not a changed MEMORY curve or a second copy of the buffer.

### Valid history and reverse motion

GhostBuffer now counts valid samples separately per channel, incrementing on
write, saturating at capacity, and returning to zero on reset. The forward read
implementation is unchanged. `isDelayRecorded(channel, d)` requires finite d,
`0 <= d <= H`, and `ceil(d) < validSamples[channel]`, protecting fractional reads.

A new region is allowed only when `S + N + A + 2` samples of delay are recorded
in BOTH channels. Otherwise startup is silent; at later handovers a previously
valid MEMORY target is reused until the requested deeper region becomes valid.
An incomplete region is never filled with unrecorded audio. Both heads also
validate each actual read; tests assert that scheduled reads never need rejection.

For region age k (output samples since its creation), and shared DRIFT offset o:

```text
readDelay[k] = S + 2*k + o[k]
sourceTime[k] = currentWriteTime[k] - readDelay[k]
sourceTime[k+1] - sourceTime[k] = -1 - (o[k+1] - o[k])
```

Thus DRIFT zero produces actual -1-sample source motion; nonzero DRIFT retains
fractional interpolation. Reverse speed magnitude is `1 + derivative(o)` rather
than forward's `1 - derivative(o)`: cents depth stays the same but pitch polarity
is inverted. The same per-voice LFO drives both region heads and both channels.

### Region and mode transitions

When an outgoing region has **50 ms** remaining, the next fully recorded region
starts at its newest edge. Both heads run backward during the overlap. The blend
uses `w = 0.5 - 0.5*cos(pi*t)`, `output = old*(1-w) + new*w`. Complementary
raised-cosine weights have smooth endpoint slopes and no correlated-signal boost.
The first valid region fades in from silence with the same 50 ms window.

MEMORY changes do not relocate an audible reverse head. The latest requested
depth/length is used at the next region handover. The original 100 ms forward
MEMORY crossfade still operates unchanged. Reverse MEMORY response is therefore
region-paced: it can take roughly one phrase (up to 1.8 s), or longer if startup
history is insufficient, to finish adopting a new deeper target. DRIFT phase is
never restarted by that change.

The existing REVERSE boolean drives a **100 ms linear mode crossfade** inside each
voice between its forward and reverse reads, before the existing gain/pan and
BLUR processing. Repeated same-valued automation does not restart the ramp.
Both paths stay warm while either is inaudible. At settled OFF the reverse blend
is skipped exactly, preserving the previous arithmetic. Only prepare/reset clear
region state; a mode toggle does not reset MEMORY, DECAY, DRIFT or BLUR.

The processor caches the existing APVTS `reverse` parameter and reads it in
prepare/process. Project load with REVERSE enabled initializes mode to ON and
waits for valid history; there is no startup forward leak. The boolean still
saves/restores through the existing state serialization. No new parameter or UI
was added.

### Feedback, stereo and limitations

Forward/reverse mode blending feeds the existing per-voice BLUR low-pass,
summed diffusion, wet output and normalized DECAY return. There is no separate
reverse filter, independent feedback reversal, cross-channel routing, or change
to the 0.95 coefficient/tanh protection. Reversed material naturally becomes
part of future recorded memories. Dry output remains forward and unchanged.

The feature reverses overlapping remembered fragments, not an entire recording
or phrase detected by content. Cold startup must collect enough history before
playing, and the first tiny transient may be partly missed while regions become
valid. Overlaps can colour/cancel some material. Deep long regions may need the
newer anchor described above, and MEMORY automation follows region boundaries.
Keeping both modes warm adds bounded interpolation work even with REVERSE OFF;
no audio callback allocation, resizing, locks, I/O or randomness is introduced.
The 1400-second host tail, UI, feedback structure and BLUR are unchanged.

## DAW test procedure

1. Close the DAW before replacing a previously loaded plugin. Build, then copy
   the **entire** `build/GHOST_artefacts/Release/VST3/GHOST.vst3` folder to
   `C:/Program Files/Common Files/VST3/GHOST.vst3`. Reopen the DAW and rescan VST3s.
2. Use a stereo track with one short dual-mono click or drum hit peaking around
   -12 dBFS, followed by at least 12 seconds of silence. Insert GHOST. Keep playback
   running through the silence; avoid looping so old events do not confuse the test.
3. Set REVERSE OFF, and DECAY, DRIFT and BLUR to 0%. Set MIX to 0%, wait at least 150 ms, and play the hit: only the original should
   sound. For a dry null test, invert polarity on an aligned duplicate without
   GHOST; the two tracks should cancel at steady MIX 0%.
4. Set MIX to 100%, MEMORY to 50%, and wait 250 ms before the hit. Hear no immediate
   hit, then four ghosts at about 0.80, 1.56, 2.56 and 3.72 seconds. They alternate
   left/right emphasis and get quieter. There must be no additional feedback echoes.
5. Repeat the isolated hit at MEMORY 0%, 25%, 75% and 100%, allowing the previous
   tail to finish and waiting 250 ms after each setting change. Compare with the
   delay table above. At 100%, keep transport running past the last 9.3-second hit.
6. Set MIX to 35% and play a sparse phrase: hear the live source plus the four
   ghosts. On sustained audio, sweep MEMORY slowly and then make abrupt 0/100%
   moves. Listen for smooth handovers without sharp clicks or big pitch slides.
   Automate MIX as well to check its preserved 50 ms ramp.
7. Test a left-only hit, then a right-only hit, with room for each tail. Wet audio
   stays on its originating channel. For dual-mono hits, the four voices should
   alternate stereo emphasis. Play for over 20 seconds to cross ring wraps.
8. Repeat at 44.1, 48, 88.2 and 96 kHz, with host blocks such as 64 and 512. Times
   should match the table at every rate. Keep DECAY at zero for these baseline
   checks. The UI layout remains unchanged.

## FL Studio DECAY listening test

1. Close FL Studio and copy the complete new `GHOST.vst3` bundle to
   `C:/Program Files/Common Files/VST3/GHOST.vst3`. Reopen FL Studio. Use
   **Options > File settings > Manage plugins**, enable **Verify plugins**, and
   **Find more plugins** if GHOST needs rescanning. Load it into an empty Mixer
   effect slot on the track receiving your test clip. Leave FL's effect-slot
   mix at 100% so only GHOST's own MIX controls the blend.
2. Use one short dual-mono hit peaking near -12 dBFS. Work in Song mode and leave
   at least 60 seconds of silence after it; keep transport running. Avoid looping
   and other delay/reverb effects so regenerated audio is easy to identify.
3. Start with GHOST MIX 100%, MEMORY 50%, DECAY 0%, DRIFT 0%, BLUR 0%, settled before the hit.
   Expect only four initial arrivals at 0.80/1.56/2.56/3.72 seconds.
4. Repeat with DECAY 25%, 50%, 75% and 100%. Compare persistence after four seconds:
   each step should regenerate more, with 75-100% building a denser, longer trail.
   Use a fresh plugin instance between passes for a guaranteed empty history;
   simply stopping transport or turning DECAY down may not clear it.
5. Try MEMORY 0%, DECAY 100% for closely spaced regeneration. Then try MEMORY 100%,
   DECAY 100% and listen through at least 60 seconds (three minutes for a longer
   check). The first ghosts arrive at 2.0/3.9/6.4/9.3 seconds; later ghosts should
   become denser and progressively fade without runaway volume.
6. Set MIX 0% during a tail: only live dry input remains audible, but regeneration
   continues internally. Bring MIX back up to reveal it. Sweep DECAY between 0%
   and 100% over sustained audio, then stop the source while leaving transport
   running. Expect smooth transitions; pre-existing memories still play out.
7. Test left-only then right-only hits in fresh instances. No return should cross
   into the silent channel. Repeat at 44.1/48/88.2/96 kHz and host blocks 64/512.

FL Studio references: [plugin installation](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm)
and [Mixer effect slots](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/mixer_plugin.htm).

## FL Studio DRIFT listening test

1. Close FL Studio, replace the complete installed VST3 bundle with the new build,
   reopen and rescan if needed as described above. Route a sustained mono vocal,
   held instrument note, or 220 Hz sine equally to both channels of a Mixer track.
   Load GHOST in an effect slot and keep FL's slot mix at 100%.
2. Set GHOST MEMORY 50%, DECAY 0%, MIX 100%, DRIFT 0%, BLUR 0%. Play for at least five
   seconds so all voices arrive. The sustained pitch should remain steady.
3. Try DRIFT 25%, then 50%, then 100%, listening for at least 20 seconds at each
   setting. Expect subtle movement at 25%, slow independent beating/detuning at
   50%, and strong smooth warping at 100%. Do not judge only during the 150 ms
   knob transition. Both stereo channels of an individual ghost move coherently.
4. Set MIX 35% to hear the unaffected dry source against the drifting ghosts.
   At MIX 0%, DRIFT must have no audible effect on the dry signal.
5. With DRIFT 100%, change MEMORY between 0% and 100% while the source plays.
   Listen for smooth handovers without clicks or LFO restarts. Automate DRIFT
   0% to 100% and back as well; rapid jumps may briefly bend pitch more strongly.
6. In a fresh instance, set MEMORY/DECAY/DRIFT/MIX all to 100%. Play a short hit or
   vocal burst near -12 dBFS, then silence with transport running for three minutes.
   Expect long, evolving ghosts that trend downward, not runaway volume.
7. Repeat with MEMORY 0%, plus separate left-only and right-only sources, and with
   sample rates 44.1/48/88.2/96 kHz and buffer sizes 64/512. There should be no
   leakage to the silent channel, boundary clicks, or change in modulation rate.

## FL Studio BLUR listening test

1. Close FL Studio, replace the complete installed `GHOST.vst3` bundle with the
   Release build, reopen and rescan if required. Route the source to a Mixer track
   with GHOST in an otherwise empty effects chain. Keep FL's effect-slot mix at
   100%; use GHOST's own MIX. Disable the wrapper's Smart disable for long-tail
   listening if FL stops processing silent input. Keep transport running.
2. Start at MEMORY 50%, DECAY 0%, DRIFT 0%, MIX 100%, BLUR 0%. Use isolated drum
   hits/claps near -12 dBFS, separated by at least six seconds. Check the familiar
   four arrival regions at 0.80/1.56/2.56/3.72 seconds.
3. Repeat at BLUR 25/50/75/100%, waiting at least 200 ms after knob changes. Expect
   progressively softer attacks and increasing short smear, not an added long
   echo. Compare at comfortable levels; do not confuse a level change with smear.
4. Repeat with spoken phrases, sustained vocals, and held synth notes. At 50%
   words/notes should remain identifiable with a softer outline; at 100% the
   texture is darker and washed out, but the body should remain audible. Check
   headphones/stereo and mono for objectionable ringing or cancellation.
5. Set MIX 35% to hear clear live audio against blurred ghosts. Set MIX 0% and
   automate BLUR: the original must remain unchanged. At MIX 100%, move BLUR
   repeatedly between 0% and 100%; listen for clicks/zipper noise.
6. Enable DRIFT 50% and DECAY 75%: regenerated material should inherit softness
   while DRIFT keeps its slow pitch motion. Then use a fresh instance with all
   five controls at 100%, feed one short burst, and leave transport running for
   three minutes. The long tail should trend down without runaway level.
7. Repeat with MEMORY 0% and maximum BLUR/DECAY/DRIFT. Test left-only/right-only
   events for no leakage, and sample rates 44.1/48/88.2/96 kHz with host blocks
   64/512. The 1400-second host-tail declaration remains; use an explicit render
   end if exporting a listening test.

FL effect-slot mix and Smart disable controls are described in the
[official Mixer effect-slot manual](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/mixer_plugin.htm).

## FL Studio REVERSE listening test

1. Close FL Studio, replace the complete installed GHOST.vst3 bundle with the
   new Release bundle, reopen and rescan if needed. Load GHOST on the source's
   Mixer insert. Leave FL's effect-slot mix at 100% and keep transport running.
2. Set MEMORY 50%, DECAY/DRIFT/BLUR 0%, MIX 100%, REVERSE OFF. Play an isolated hit
   followed by silence: the original forward arrivals remain around
   0.80/1.56/2.56/3.72 seconds.
3. Play a spoken phrase repeatedly for at least ten seconds to populate history,
   then enable REVERSE. The wet memories should become backward fragments and
   swells. At MIX 35%, the live phrase must still sound forward; at MIX 0%,
   toggling REVERSE must have no audible effect on dry input.
4. Toggle OFF/ON/OFF on a sustained vocal or synth note. Change MEMORY 0/50/100%
   while reversed. Listen for smooth handovers rather than clicks; MEMORY may
   take up to a phrase to update. Try DRIFT 100% and BLUR 50-100% to check their
   preserved motion and softening.
5. Save a project with REVERSE ON, close/reopen it, and confirm the checkbox stays
   enabled. Playback starts with silent ghosts until enough history is recorded;
   live dry input remains available immediately.
6. In a fresh instance, set MEMORY/DECAY/DRIFT/BLUR/MIX all to 100%, REVERSE ON.
   Feed a short vocal-like burst or drum hit near -12 dBFS, then silence, leaving
   transport running for three minutes. Compare the fading tail with forward
   mode. Also try rhythmic hits and a sustained tone; avoid other delay/reverb FX.
7. Test left-only then right-only clips for no leakage, and repeat at
   44.1/48/88.2/96 kHz and host blocks 64/512. Disable FL Smart disable if it cuts
   silent-input processing. Use an explicit export end; host-tail reporting is
   intentionally still 1400 seconds. Recorded-material listening remains manual.

## Run DSP tests

Using the workspace-local tools (or substitute cmake/ctest from PATH):

```powershell
& .\.tools\cmake-package\cmake\data\bin\cmake.exe -S . -B build -A x64 -DGHOST_BUILD_TESTS=ON
& .\.tools\cmake-package\cmake\data\bin\cmake.exe --build build --config Release --target GHOST_DSPTests GHOST_ProcessorTests GHOST_UITests --parallel 2
& .\.tools\cmake-package\cmake\data\bin\ctest.exe --test-dir build -C Release --output-on-failure
```

Tests cover integer/fractional reads, both sides of the wrap seam, ten-second
history at all four sample rates, stereo isolation, reset, delay clamping,
all five MEMORY knots, four distinct impulse arrivals before and after wrapping,
gain/pan, wet headroom, finite output, absence of feedback, exact dry preservation,
wet-only output, continuous capture at 0%, 50 ms MIX smoothing, repeated MEMORY
automation and crossfade settling, re-prepare, and processing block sizes 64/127/512.
The previous tests explicitly set DECAY 0% to retain their regression meaning.

DECAY tests add all five knob settings, 100 ms coefficient ramps, 60-second
short-memory sessions, 180-second maximum-memory sessions, left/right-only input,
60 seconds of sustained full-scale stereo input with aggressive automation,
MIX-independent feedback, NaN/infinity/extreme-input containment, reset, and
block-size independence. Every output sample is checked for finite/bounded values.

## Verification

The Windows x64 Release VST3 build succeeded with Visual Studio Build Tools 2019
(MSVC 19.29), Windows SDK 10.0.19041.0, and CMake 3.31.6. JUCE's VST3 manifest
helper successfully loaded the generated plugin and created its module metadata.
The DSP regression suite passed in Release at all four supported sample rates.
DAW playback and interactive editor testing have not been performed.

DECAY verification (Release, 2026-10-02): full suite passed in approximately
20.8 seconds. Build produced no new warnings. Each rate ran a 100 ms stereo
burst (left 0.5, right -0.35) followed by silence at MEMORY/DECAY/MIX 100% for
180 seconds, plus the separate full-scale sustained-input stress test:

| Sample rate | Burst peak | Energy, 50-60 s | Energy, 170-180 s | Final-window peak | Sustained stress peak |
| --- | --- | --- | --- | --- | --- |
| 44.1 kHz | 0.104428 | 0.000145649 | 0.00000631697 | 0.00073949 | 0.871744 |
| 48 kHz | 0.104428 | 0.000145651 | 0.00000631695 | 0.00073949 | 0.871744 |
| 88.2 kHz | 0.104428 | 0.000145658 | 0.00000631688 | 0.00073949 | 0.871744 |
| 96 kHz | 0.104428 | 0.000145659 | 0.00000631687 | 0.00073949 | 0.871744 |

Energy is sum of squared stereo samples divided by sample rate. Final-window
energy is about 4.34% of the 50-60 s energy, confirming downward trend; the final
peak is about -62.6 dBFS. The worst wet-output stress peak was **0.871744
(-1.19222 dBFS)**; the dry-only stress path remained exactly +/-1 by design.
No non-finite samples, runaway growth, or cross-channel feedback were observed.

DRIFT verification (Release, 2026-10-02): full regression and expanded suite
passed in approximately 39.2 seconds, with no new build warnings. Previous
MEMORY/DECAY tests run with DRIFT explicitly zero. New tests measure actual
read-head velocity over more than one complete cycle of every voice, verify
distinct trajectories, 150 ms depth ramps, phase continuity, both crossfade-head
bounds, tone modulation and stereo coherence, block-size independence, and
reset/re-prepare. Minimum-MEMORY and maximum-MEMORY feedback cases include
left-only/right-only input and invalid parameter injection.

At every sample rate, measured cents ranges were 0/0 at DRIFT 0%,
-3.125/+3.11937 at 25%, -12.5/+12.4104 at 50%, and -50/+48.5964 at 100%.
All-max MEMORY/DECAY/DRIFT/MIX burst-and-silence results:

| Sample rate | 180 s burst peak | Energy, 50-60 s | Energy, 170-180 s | Final-window peak | Sustained stress peak |
| --- | --- | --- | --- | --- | --- |
| 44.1 kHz | 0.104428 | 0.000117602 | 0.00000631170 | 0.000677694 | 0.871744 |
| 48 kHz | 0.104428 | 0.000117602 | 0.00000631170 | 0.000677694 | 0.871744 |
| 88.2 kHz | 0.104428 | 0.000117603 | 0.00000631166 | 0.000677690 | 0.871744 |
| 96 kHz | 0.104428 | 0.000117603 | 0.00000631165 | 0.000677690 | 0.871744 |

The burst is 100 ms at left 0.5/right -0.35, followed by silence. Its peak is
approximately -19.62 dBFS; final-window energy drops to about 5.37% of the 50-60 s
window. For direct comparison with the prior -1.19 dBFS result, the separate
60-second full-scale sustained/automation test again peaks at 0.871744
(-1.19222 dBFS): no material increase. All samples remain finite and bounded.
No additional output clipping or altered feedback gain was used. DAW listening
and performance profiling on other machines remain unverified.

BLUR verification (Release, 2026-10-02): the complete expanded suite passed in
approximately **59.2 seconds**, with no new build warnings. Prior MEMORY, DECAY
and DRIFT tests run with BLUR explicitly zero. New tests cover five BLUR amounts
at every rate, exact bypass, digital -3 dB cutoff, high-frequency attenuation,
impulse temporal spread, stereo decorrelation and mono sum, 150 ms automation,
reset, block sizes 64/127/512, numerical fault handling, both one-sided inputs,
minimum MEMORY, and maximum MEMORY/DECAY/DRIFT/BLUR/MIX long sessions.

At 48 kHz, the impulse energy centroid moved from 0 ms at BLUR zero to 0.177,
4.210, 27.590 and 38.926 ms at 25/50/75/100%. Impulse peak dropped from 1.0 to
0.6655/0.3906/0.1348/0.0957. Voice-filter gain at 10 kHz decreased monotonically
from 1.0 to 0.7408/0.5513/0.3543/0.2096. The other rates showed the same trends.

All-max 180-second burst-and-silence tests (same 100 ms stereo burst as before):

| Sample rate | Burst peak | Burst dBFS | Energy, 50-60 s | Energy, 170-180 s | Sustained stress peak / dBFS |
| --- | --- | --- | --- | --- | --- |
| 44.1 kHz | 0.187222 | -14.5529 | 0.000113182 | 0.00000669330 | 0.873868 / -1.17108 |
| 48 kHz | 0.187044 | -14.5611 | 0.000113181 | 0.00000669338 | 0.874213 / -1.16765 |
| 88.2 kHz | 0.187155 | -14.5560 | 0.000113181 | 0.00000669337 | 0.875501 / -1.15487 |
| 96 kHz | 0.187206 | -14.5536 | 0.000113181 | 0.00000669335 | 0.877688 / -1.13320 |

Final-window energy is approximately **5.91%** of the 50-60 s window, and final
peak is about 0.0007003. The tail trends downward at all four rates. All samples
are finite and bounded. The worst sustained full-plugin wet peak is 0.877688,
only about 0.059 dB above the earlier 0.871744 / -1.19222 dBFS result.

The higher burst peak was investigated, not clipped away: a separate isolated
BLUR-module test with a 100 ms rectangular pulse at peak 0.5 produced peaks
around 0.894-0.895 but output/input energy ratios of 0.999205-0.999209. Fixed
all-pass phase spreading redistributes energy and creates transient overshoot;
it does not amplify total energy. After the existing voice levels, the complete
engine's corresponding burst peak stays below 0.188. This is why peak and energy
are both tested. No hard limiter, feedback gain increase, or new saturation was
introduced for BLUR. Listening with recorded musical material remains pending.

## REVERSE verification results

Release VST3, GHOST_DSPTests and GHOST_ProcessorTests build successfully. The
complete DSP suite passed (336.23 seconds); processor state/automation tests also
passed through CTest at all four rates. Final incremental Release build reported
no warnings. No UI, feedback gain, BLUR algorithm or host-tail changes were made.

| Rate | 180 s all-max burst peak | dBFS | Energy 50-60 s | Energy 170-180 s | Late/early | Sustained/automation peak (dBFS) |
| --- | --- | --- | --- | --- | --- | --- |
| 44.1 kHz | 0.116825 | -18.6493 | 2.58834e-5 | 2.08865e-6 | 0.0806947 | 0.874704 (-1.16278) |
| 48 kHz | 0.116871 | -18.6458 | 2.58926e-5 | 2.08942e-6 | 0.0806958 | 0.873440 (-1.17534) |
| 88.2 kHz | 0.116875 | -18.6456 | 2.59260e-5 | 2.09215e-6 | 0.0806969 | 0.872333 (-1.18635) |
| 96 kHz | 0.116890 | -18.6445 | 2.59276e-5 | 2.09227e-6 | 0.0806966 | 0.874433 (-1.16547) |

All tails trend downward, with finite output. Final-window peaks range from
0.000415827 to 0.000416180. Compared with the preceding BLUR stage, absolute burst
peak and late energy are lower; the late/early ratio is about 8.07%, versus 5.91%.
The ratio alone is not an absolute loudness comparison.

An asymmetric recorded ramp proves actual backward ordering: outside region
crossfades the unmodulated source position advances exactly -1 sample per output
sample (tolerance 1e-7). Tests cover all four voice configurations at MEMORY
0/50/100%, startup, reset, wraparound and both active heads; rejected reads remain
zero. DRIFT tests verify negative source velocity with the expected pitch range.

The smooth dual-sine boundary/toggle/MEMORY automation test measured maximum
adjacent-sample changes of 0.000280410, 0.000257656, 0.000140257 and 0.000128858
at 44.1/48/88.2/96 kHz respectively (limit 0.003). These are signal-specific
measurements, not universal click guarantees. Synthetic speech-like bursts,
drum impulses, sustained tones and rhythmic impulses pass at every rate, as do
block-size independence, dry bypass, stereo and one-sided input checks. Existing
forward regression tests pass with REVERSE disabled. Processor tests verify the
existing eight-parameter layout, enabled project-load state, automation and save/
restore round trips; the 1400-second tail declaration is unchanged.

Recorded-material listening and an actual FL Studio project round trip remain
manual validation; automated processor tests do not claim to substitute for them.

JUCE build reference: https://github.com/juce-framework/JUCE/blob/8.0.12/docs/CMake%20API.md


## FREEZE architecture

FREEZE adds an immutable **copy-on-write snapshot** plus a separate ten-second
**working history**. It does not stop the live history cursor and does not make
an eager ten-second copy or maintain a mirror that becomes stale after release.

`GhostBuffer::prepareCapture()` allocates stereo capture samples and 64-bit
per-slot generation tags during prepare. Activation increments a generation and
records the live write cursors and valid counts. Before each live slot is first
overwritten in that generation, its old value is preserved in capture storage.
A captured read selects the preserved slot if tagged, otherwise the untouched
live slot. Thus activation is constant work, subsequent writes preserve at most
one float/tag per channel per frame, and rapid recaptures need no bulk resync.
Fractional reads interpolate two valid captured samples. No callback allocation,
resizing, locks, file access, logging or randomness was introduced.

The snapshot contains **the actual dry-plus-regenerated history**, not dry-only
input. A two-second capture has two seconds of valid source material, not ten.
`GhostFreeze` seeds working history logically from the capture: older reads use
the immutable source until replaced by newly appended working samples. Seeding
clears only counters/cursors, not the large arrays. Previously used storage
cannot become readable merely because a new capture is shorter.

### Persistent playback and feedback

The captured samples replay in chronological order into working history. For
capture length `L` samples, the loop overlap is:

```
F = min(round(sampleRate * 0.050), floor(L / 4))
loop period = L - F
weight(t) = 0.5 - 0.5 * cos(pi * t)
```

The last `F` samples overlap the first `F` with complementary weights; playback
then resumes at position `F`. For extremely short captures `F` may be zero.
An initial 150 ms raised-cosine splice joins the newest captured sample to this
replay stream, preventing a discontinuity at the first newly appended sample.
The brief endpoint hold is only part of the memory excitation splice; live dry
output is never held or delayed.

Working writes are:

```
frozen working input = replayed immutable capture + normalized DECAY return
live history input   = new live dry input + normalized DECAY return
```

After the source handover reaches fully frozen, a 100 ms linear ramp admits new
regeneration. It ramps out on release. DECAY itself retains its existing 100 ms
smoothing, `0.95*d*(2-d)` mapping, channel normalization and tanh protection.
The original capture is never modified by regeneration. At DECAY zero the
captured loop persists; higher DECAY adds evolving regenerated layers rather
than defining the lifetime of that immutable source. No output limiter was added.

### Source transitions and controls

Both ON and OFF use a **150 ms complementary linear reader crossfade** before
BLUR. Activation copies only fixed voice state, including current delay heads,
reverse regions, LFO phase and smoother state. Both reader sets initially inherit
the captured history; the outgoing readers also use its advancing working view
while held. This deliberately excludes new live input even during engagement:
a live-reading fade leg could otherwise leak new notes through shared BLUR state
into frozen feedback. At engagement the histories are identical, so this is a
transparent state handover, with the replay excitation splice smoothing the new
continuation. On release the outgoing readers return to real live history under
the 150 ms crossfade. At fully frozen the blend explicitly selects the frozen
sample to avoid cancellation-rounding dependence on another source.

An OFF handover finishes before a rapid ON request is accepted. Such requests
coalesce, and the next capture represents live history at the actual capture
frame (up to 150 ms later), not an earlier overwritten snapshot. Parameter
changes are sampled by the processor at block boundaries, as before.

Frozen MEMORY uses the existing nonlinear depth `D`, constrained to captured
source duration before applying the four voice ratios:

```
frozenDepthSeconds = min(D, capturedSamples / sampleRate)
voiceDelaySeconds  = frozenDepthSeconds * voiceRatio
```

The existing 100 ms forward MEMORY transitions and region-safe reverse handovers
remain. Deep settings saturate for short captures. Working valid-history counts
increase only as actual replay/feedback samples are appended. If a reverse region
still cannot fit available working history, it waits safely; particularly short
captures can therefore take time to develop all four reverse voices. Unavailable
forward history is silence. There is no invalid/unwritten access.

The existing reverse engine reads working history without a second reverse
implementation. DRIFT phases are copied rather than reset, and its mapping and
smoothing are unchanged. Both sources enter the **same existing per-voice BLUR
filters and shared diffusion**, then wet output and DECAY feedback. There is no
second BLUR bank, new crossfeed, channel swap, or change to voice pan/gains.
The dry branch remains immediate, forward, and controlled only by normal MIX.

### Startup, reset and state

The existing `freeze` boolean is cached by the processor and forwarded during
prepare and processing. It saves/restores/automates through the existing APVTS;
the parameter layout still has eight entries. Captured audio is **not serialized**.
Loading with FREEZE ON captures empty history before the first input frame, so
wet output remains silent while dry input works. Switch OFF, supply audio, then
switch ON to capture. New live input does not automatically fill an empty capture.
Reset clears live/working history, capture validity, feedback, BLUR, voice state
and transitions. If FREEZE remains ON, the next capture is empty and safe.
The 1400-second host-tail declaration remains unchanged, even though frozen
playback may persist indefinitely; correcting that declaration is deferred.

### Memory and CPU

For `N = ceil(10*sampleRate)+1` stereo frames, added large storage is:

- Captured samples: `8*N` bytes.
- Two 64-bit tag arrays: `16*N` bytes.
- Frozen working samples: `8*N` bytes.

Total: `32*N` bytes, plus small vector/cursor and four extra fixed voice states.

| Sample rate | Added bytes | Decimal MB | MiB |
| --- | --- | --- | --- |
| 44.1 kHz | 14,112,032 | 14.112 | 13.458 |
| 48 kHz | 15,360,032 | 15.360 | 14.648 |
| 96 kHz | 30,720,032 | 30.720 | 29.297 |

FREEZE OFF adds a few predictable state checks; it does not run the extra voices
or touch capture sample/tag arrays. Active FREEZE adds a second set of voice
reads, bounded capture preservation, and replay/working-buffer traffic. This
materially increases voice/memory work, but not BLUR filter/diffusion count.
No host CPU percentage benchmark is claimed. No press-time work scales with the
ten-second buffer length. Storage is allocated on prepare, not parameter changes.

### FL Studio FREEZE listening test

1. Close FL Studio and replace the complete installed `GHOST.vst3` bundle with
   `build/GHOST_artefacts/Release/VST3/GHOST.vst3`, normally under
   `C:/Program Files/Common Files/VST3/`. Reopen and rescan if needed.
2. Insert GHOST on a mixer track and leave the host effect-slot mix at 100%.
   Start with FREEZE/REVERSE OFF, MEMORY 50%, DECAY/DRIFT/BLUR 0%, plugin MIX 100%.
3. Play a distinctive vocal phrase or melody for at least 12 seconds. Enable
   FREEZE, then stop that source. Its remembered material should continue.
4. Play a completely different phrase or instrument. At MIX 100% the frozen
   texture should remain the original capture. Set MIX 35% to hear new forward
   dry material over it; MIX 0% should sound exactly dry regardless of FREEZE.
5. While frozen, move MEMORY through 0/50/100%, toggle REVERSE, then introduce
   DRIFT and BLUR. The same captured source should remain usable without clicks.
6. Compare DECAY 0/50/100%. At zero the captured source persists; higher amounts
   add regeneration. Return FREEZE OFF: within the source fade the ghosts should
   return to recent live history. Re-enable it after 150 ms to capture new material.
7. With all active controls at maximum and REVERSE/FREEZE ON, stop input and listen
   for three minutes. Expect a bounded persistent texture, not mandatory silence.
   Disable FL Studio Smart Disable during the test so it cannot suspend processing.
8. Save with FREEZE ON and reopen the project. The checkbox should restore, but
   captured audio should not. Confirm safe wet silence and working dry audio;
   switch OFF, play material, and switch ON to create a new capture.
9. Repeat with left-only/right-only input and supported project sample rates.
   Try small and large audio buffers; toggles are block-quantized but the DSP
   evolves sample-by-sample.

Known limitations: captured audio does not survive reload/reset; empty captures
require explicit recapture; short histories limit MEMORY exploration and may
delay reverse startup; crossfaded loop seams overlap/skip 50 ms at most rather
than replaying every sample exactly once; very rapid toggles are coalesced;
active FREEZE costs additional memory/CPU; the old host-tail declaration does
not describe an indefinitely held capture. Actual FL Studio listening remains a
manual check, separate from automated DSP and processor validation.

### FREEZE verification results

Release VST3 and both test executables built without warnings introduced by this
stage. The complete CTest run passed: GHOST_DSPTests in 486.40 seconds and
GHOST_ProcessorTests in 1.50 seconds, 2/2 tests passed (487.91 seconds total).
The retained console report is `freeze-verification.log`.

Each stress case populates live history for 12 seconds, engages FREEZE, then
processes 180 additional seconds with MEMORY/DECAY/DRIFT/BLUR/MIX at maximum and
REVERSE enabled. Times below are measured from engagement. Energy is mean square
over both stereo channels in the indicated ten-second window.

| Rate | Frozen peak | dBFS | Energy 50-60 s | Energy 170-180 s | Late/early | Transition max step |
| --- | --- | --- | --- | --- | --- | --- |
| 44.1 kHz | 0.697559 | -3.12838 | 0.0255171 | 0.0220662 | 0.864759 | 0.000144351 |
| 48 kHz | 0.697102 | -3.13407 | 0.0255056 | 0.0220592 | 0.864877 | 0.000132442 |
| 88.2 kHz | 0.695974 | -3.14813 | 0.0255381 | 0.0219094 | 0.857911 | 0.000072185 |
| 96 kHz | 0.695503 | -3.15402 | 0.0255353 | 0.0219086 | 0.857973 | 0.000066340 |

All output remained finite and bounded. Late-window energy is lower in these
cases, but the expected behavior is a persistent texture, not eventual silence.
Transition measurements use the smooth deterministic dual-sine test; they are
not universal bounds on arbitrary input transients.

At every sample rate, frozen wet output with unrelated new live input matched
the silence-input reference exactly (maximum difference 0), in both playback
directions at DECAY 0/50/100%. MIX zero independently reproduced new dry input.
Known fractional ramp reads survived complete live-ring overwrites; short valid
counts, recapture, startup enabled, reset, one-frame captures and rapid toggles
passed. Left-only/right-only inputs produced no channel crossfeed.

Automation renders were sample-identical across host block sizes 32, 64, 256,
512 and 1024, with events aligned to the same sample positions. Existing forward,
REVERSE, MEMORY, DECAY, DRIFT and BLUR regression suites also passed with FREEZE
OFF. APVTS tests passed enabled-load, OFF/ON automation, reset and state round
trips at all four rates; serialized state stayed below 8 KiB with no audio blob.


## GHOST macro architecture

The processor forwards the existing GHOST APVTS value to `GhostEngine::setGhost`.
No parameter IDs, ranges, defaults or visible individual values change. The new
`GhostMacro` contains a normalized 150 ms linear ramp and pure scale mappings.
The standalone engine starts at unity for existing DSP callers; the plugin always
forwards its actual parameter, including the unchanged **35% default**, before
prepare. Every historical regression test therefore runs at GHOST 100%.

Let `g = clamp(userGhost / 100, 0, 1)` and let `s` be its sample-by-sample 150 ms
smoothed value. The final formulas are:

| Quantity | Formula | Smoothing |
| --- | --- | --- |
| Wet field amplitude | `s*s` | 150 ms macro ramp |
| Voice pan offset | `basePan * s*sqrt(s)` | Same macro ramp |
| Effective DRIFT input | `userDrift * g` | Existing 150 ms depth smoother |
| Effective BLUR input | `userBlur * g*sqrt(g)` | Existing 150 ms BLUR smoothers |
| Effective DECAY input | `d = userDecay * g*g` | Existing 100 ms coefficient smoother |
| Feedback coefficient target | `0.95*d*(2-d)` | Unchanged DECAY mapping |

User DRIFT/BLUR/DECAY inputs in these formulas are normalized 0-1. Existing
module smoothers receive targets derived from `g`, not continuously retargeted
values from `s`; this avoids cascading ramps and per-sample filter-coefficient
recalculation. All quantities stay inside their previously verified ranges.
At a settled value, nominal DRIFT cents are `50*(userDrift*g)^2`; LFO rates and
phase relationships remain unchanged. For example, DRIFT 60% with GHOST 50%
uses effective DRIFT 30%, without moving the visible 60% control.

| GHOST | Wet amplitude | Level change | Width/BLUR scale | DRIFT scale | DECAY-input scale |
| --- | --- | --- | --- | --- | --- |
| 0% | 0 | silence | 0 | 0 | 0 |
| 25% | 0.0625 | -24.08 dB | 0.125000 | 0.25 | 0.0625 |
| 50% | 0.2500 | -12.04 dB | 0.353553 | 0.50 | 0.2500 |
| 75% | 0.5625 | -5.00 dB | 0.649519 | 0.75 | 0.5625 |
| 100% | 1 | 0 dB | 1 | 1 | 1 |

### Routing and endpoints

The base voice levels remain -12/-15/-18/-21 dB. `GhostVoice` changes only each
voice's constant-power left/right balance using its scaled pan offset; it does
not sum channels or introduce crossfeed. Width is cached, so trigonometric pan
recalculation occurs only while width changes. At zero the pan offsets are
centered; existing stereo differences in source material are not converted to
mono. Dry stereo is never processed by this width control.

```
live or frozen history
  -> existing forward/reverse voices, effective DRIFT, scaled pan offsets
  -> existing per-voice BLUR filters
  -> existing shared diffusion
  -> multiply wet field by s*s
       -> unchanged equal-power MIX -> output
       -> existing normalized/tanh-protected DECAY feedback
```

Applying level after diffusion guarantees that old diffuser/filter tails cannot
leak through a settled macro zero. Feedback receives the same scaled field, so
lower macro values reduce regeneration both through effective DECAY input and
through wet excitation level. The coefficient is never scaled above 0.95; the
normalization constants, tanh protection and feedback routing are unchanged.
No limiter, additional audio buffer or new effect algorithm was introduced.

At GHOST 100%, mapping endpoints are exactly unity and the final wet multiply is
bypassed to preserve previous arithmetic. Full original pan positions and
DRIFT/BLUR/DECAY settings are restored. Transition histories can still differ
after automation because past regenerated material differs; endpoint regression
uses identically initialized configurations, not an expectation of erasing history.

MEMORY depth and its transitions are independent of the macro. MIX is not changed:
at MIX 100% and settled GHOST 0%, output is silence; at MIX 0%, output is exactly
dry. At intermediate MIX, macro zero leaves the dry component at its normal
MIX attenuation. Macro zero is **not** a dry-through bypass switch.

REVERSE and FREEZE continue processing while muted. No LFO, reverse head, frozen
capture or working-history reset is caused by macro changes. Frozen replay remains
available at zero, and returns smoothly when GHOST rises. DECAY evolution is
intentionally reduced while muted, so the returned texture need not equal a run
that kept the macro at 100% throughout. The original immutable capture survives.

### CPU and real-time behavior

Added per-frame work is one macro ramp, a square root/multiplications, cached
width checks, and stereo output multiplication except at unity. Moving width
recalculates sine/cosine pan gains for each active reader set. Effective modulation
and filter targets update only when user controls change; their existing sample
smoothers do the rest. There is no new allocation in processing, no resizing,
locking, I/O, logging, random-device use, or added large memory storage. No host
CPU percentage benchmark is claimed. Muting does not suspend voices or FREEZE,
so GHOST zero is not a CPU-saving bypass.

### FL Studio GHOST listening test

1. Close FL Studio, replace the complete installed GHOST.vst3 bundle with
   `build/GHOST_artefacts/Release/VST3/GHOST.vst3`, reopen, and rescan if needed.
   Leave FL's effect-slot mix at 100%.
2. Set plugin GHOST 100%, MEMORY 50%, DECAY 40%, DRIFT 60%, BLUR 80%, MIX 35%,
   REVERSE/FREEZE OFF. Play a vocal or rhythmic phrase for at least 12 seconds.
   This is the full pre-macro configured behavior.
3. Move GHOST through 75/50/25/0%. Expect decreasing ghost level, pan offsets,
   motion, blur and regeneration. Confirm the visible individual controls stay
   exactly where you set them. The live dry signal should remain forward.
4. Set MIX 100%, wait for the macro ramp: GHOST zero should give silence. Raise
   GHOST to hear wet-only material return. Set MIX zero: GHOST automation must
   make no audible difference to dry input.
5. Set GHOST 100%, MIX 100%, REVERSE ON. Play material, enable FREEZE, then stop
   the source. Set GHOST zero for five seconds; restore 100%. The frozen reversed
   texture should return without recapturing or resetting the modes.
6. With all active controls at maximum and REVERSE/FREEZE ON, automate GHOST
   between 0/50/100%, including quick movements. Listen for smooth changes and
   controlled feedback. Disable Smart Disable during a three-minute hold test.
7. Save/reopen and verify GHOST plus all individual parameter values restore.
   As before, frozen audio itself is not serialized: recapture after reopening.
8. Repeat at supported project rates and small/large audio buffers. Host control
   changes are still block-quantized; processing ramps are sample-by-sample.

Known limitations: the unchanged 35% default now attenuates wet level by about
18.24 dB and scales other contributions; use 100% for historical comparisons.
Width scales voice pan offsets, not the entire incoming stereo image. Automation
changes feedback history and cannot undo it. The existing FREEZE reload/short-
history limitations and 1400-second host-tail declaration remain. No UI, preset,
visualizer or installer changes were made. Actual musical listening in FL Studio
remains a manual validation, separate from automated tests.


### GHOST macro measurements

The macro-specific tests pass at 44.1/48/88.2/96 kHz. Settled GHOST zero with MIX
100% measured **wet RMS = 0** (exact digital silence in these tests). MIX zero
reproduced both dry channels exactly during all macro automation. Forward/reverse
and frozen processing are not suspended by this mute.

| Rate | Smooth automation max step | 180 s repeated 0/100 frozen stress peak | dBFS | Final audible-window mean square |
| --- | --- | --- | --- | --- |
| 44.1 kHz | 0.000183813 | 0.627552 | -4.04701 | 0.0193253 |
| 48 kHz | 0.000169117 | 0.626379 | -4.06325 | 0.0193144 |
| 88.2 kHz | 0.0000920668 | 0.624510 | -4.08921 | 0.0192477 |
| 96 kHz | 0.0000846125 | 0.623970 | -4.09672 | 0.0192462 |

The long automation case fills history for 12 seconds, freezes with all individual
controls at maximum and REVERSE ON, then alternates GHOST 0/100 every five seconds
for 180 seconds. Peaks include the fill interval. Final-window mean square covers
the last five audible seconds across both channels; it verifies that the captured
texture still returns after repeated muting. All output stayed finite and bounded.

The shorter smooth-signal test includes 0-to-100, 100-to-0, 0-to-50-to-100, 20 ms
rapid changes, several-second silent holds, MEMORY changes and FREEZE release.
Its measured adjacent-sample step is signal-specific, not a universal click bound.
Renders match exactly across block sizes 32/64/256/512/1024 with identical sample-
aligned events. Isolated pan tests verify centered/intermediate/full offsets;
one-sided engine tests verify no crossfeed. Mapping tests verify safe monotonic
feedback, exact unity/zero endpoints, 150 ms gain settling, invalid-input handling
and unchanged LFO phase when changing macro-derived controls.

The GHOST 100% static maximum-FREEZE test additionally checks its peak against
recorded pre-macro values with a 3e-6 tolerance; the historical measurement table
above remains the reference. Entire-suite and processor results are retained in
`macro-verification.log`.

Final verification: Release VST3 and all test executables built without warnings
introduced by this stage. Complete CTest passed **2/2 tests, zero failures**:
GHOST_DSPTests 603.73 seconds, GHOST_ProcessorTests 2.99 seconds, 606.74 seconds
total. Processor checks confirm the unchanged 35% default, working macro
automation, visible-control independence, frozen reverse persistence, state
round-trip, eight-parameter layout and unchanged 1400-second tail at every rate.
All existing MEMORY/DECAY/DRIFT/BLUR/REVERSE/FREEZE/MIX regressions passed at unity.
Static maximum-FREEZE peak remains 0.697559 (-3.12838 dBFS); worst full-engine
sustained peak in the broader legacy suite remains 0.877688 (-1.13320 dBFS).


## Static custom UI foundation

This section records the base editor stage. The animated layer and permitted
processor metering additions are documented in the following visualizer section.

The editor is fixed at **820 x 540 logical pixels**. The provided `GhostLOGO.png`
is copied unchanged to `Assets/GhostLogo.png`, embedded with JUCE BinaryData, and
decoded on the message thread. The header uses a 240 x 96 rectangle with preserved
aspect ratio and a 480-pixel-wide cached image for 2x display. The VST3 does not
need to find the original Desktop file or an external Assets folder at runtime.
No logo regeneration, borrowed symbols, external fonts or image-generation step
was used. The original sharp mint lettering and smoke establish the palette and
contrast; the controls/background are original JUCE vector drawing.

### UI classes and theme

`GhostAudioProcessorEditor` owns layout, the image caches, six `GhostKnob`
components, two standard JUCE ToggleButtons with custom painting, button
attachments, and a TooltipWindow. It contains no DSP state or processing logic.
`GhostKnob` wraps a standard rotary Slider plus its SliderAttachment. The same
class serves small dials and the large macro via a visual property; no separate
macro DSP or duplicate UI control hierarchy is necessary.

`GhostLookAndFeel` owns all drawing styles, the system sans-serif font helper,
custom rotary/button/tooltip painting and backdrop generation. Colors live in
`GhostTheme` in `Source/UI/GhostLookAndFeel.h` (opaque sRGB hex):

| Theme role | Hex |
| --- | --- |
| Black / header | #000000 |
| Background | #040908 |
| Panel | #0B1714 |
| Dial body | #101E19 |
| Deep spectral green | #123E30 |
| Edge | #275646 |
| Ectoplasm mint | #52FFC0 |
| Pale highlight | #DBFFF2 |
| FREEZE accent | #A0FFE9 |
| Primary label | #BDD9CD |
| Muted label | #74988A |

Opacity variations and gradients derive from that palette rather than scattering
new RGB literals in the editor. The logo retains its own original raster colors.
No extra font files are bundled; actual system-font rasterization can vary.

The central macro uses a dark layered disc, graduated tick marks, mint track and
active arc, pale indicator, central whole-percentage readout, and integrated
GHOST/INTENSITY labels. Glow strength rises with the value. Small dials share the
same drawing language with finer arcs, restrained hover brightening, percentage
readouts and labels. MIX's compact value area accommodates 100% without clipping.

REVERSE uses a cut-corner outline, two original backward chevrons and a faint
active directional streak. FREEZE uses a stronger pale cyan-green border, internal
illumination and two pause-like strokes. Both have readable active/inactive text
and keyboard-focus outlines. The background combines a near-black radial green
field, faint curved wisps, vertical streaks, and a restrained lower control panel.
This foundation stage had no visualizer or continuous animation; the animated
visualizer added next is documented below.

### Attachments and interaction

Six SliderAttachments bind directly to `ghost`, `memory`, `decay`, `blur`, `drift`
and `mix`. Two ButtonAttachments bind to `reverse` and `freeze`. Host automation
updates the controls, and control edits update the existing parameters. Attachment
lifetimes end before their controls; editor close/reopen does not reset parameters.
All IDs, ranges, defaults and mappings remain unchanged.

Knobs support standard JUCE rotary dragging, mouse wheel, keyboard arrows, and
double-click reset to the parameter's existing default. Percentages are rounded
for the central readout; the underlying continuous parameter value is preserved.
Hover tooltips use the requested text verbatim, including the longer MEMORY
explanation. Tab focus has a visible outline. Tooltips appear after 650 ms.
There is no separate numeric-entry textbox, new bypass, preset system or UI scale
selector in this stage.

### Rendering and DSP protection

The background is drawn from JUCE paths/gradients once into a 1640 x 1080 cache
at editor construction. It is not a raster screenshot used as the interface:
all controls/text remain individually drawn and interactive. Glow uses a few
translucent strokes and concentric shapes, never a per-frame Gaussian blur.
Only normal interaction/host updates trigger control repainting. No new audio-
callback allocation, locks, parameter layout or DSP logic was introduced.

`dsp-ui-baseline.json` retains SHA-256 hashes for all 16 DSP source/header files
and both PluginProcessor files. At completion of the static UI stage, all **18 protected files were byte-identical**.
The subsequent visualizer stage adds only metering to the two processor files;
all 16 DSP files still match this baseline.
The existing DSP and processor test source files were not edited for this stage.
Only editor/look-and-feel code, a reusable UI control, assets, CMake wiring,
documentation, and a separate UI test executable were added/changed.

### UI previews and verification

`GHOST_UITests` renders the actual JUCE editor, rather than a separate design mockup:

- `build/UI-Previews/GHOST-default.png`: all existing defaults.
- `build/UI-Previews/GHOST-active.png`: GHOST/MIX 100%, active ghost energy.
- `build/UI-Previews/GHOST-active-2x.png`: 1640 x 1080 rendering.
- `build/UI-Previews/GHOST-zero.png`: macro zero with toggles still active.

Checks cover the fixed window size, all eight attachments in both directions,
percentage units/ranges, initial values, tooltips, keyboard adjustment, reopening
against restored state, unchanged parameter count, and 1x/2x rendering. Default
and active previews were visually inspected; a clipped compact MIX readout was
corrected before final verification. The complete run is retained in
`ui-verification.log`.

Build and run all checks from the project directory:

```powershell
& .\.tools\cmake-package\cmake\data\bin\cmake.exe --build build --config Release --target GHOST_VST3 GHOST_DSPTests GHOST_ProcessorTests GHOST_UITests --parallel 2
& .\.tools\cmake-package\cmake\data\bin\ctest.exe --test-dir build -C Release --parallel 2 --output-on-failure
```

### FL Studio static UI verification

1. Close FL Studio and replace the complete installed `GHOST.vst3` bundle with
   `build/GHOST_artefacts/Release/VST3/GHOST.vst3`, normally in
   `C:/Program Files/Common Files/VST3/`. Reopen and rescan if needed.
2. Insert GHOST on a mixer track and open its native editor. Confirm the 820 x 540
   layout, original logo, six percentage dials, and two toggles. No controls should
   be clipped. Check at your normal Windows display scaling and, if available,
   100/150/200% scaling or a second monitor.
3. Confirm untouched defaults: GHOST 35%, MEMORY 40%, DECAY 40%, BLUR 20%, DRIFT
   10%, MIX 35%, REVERSE/FREEZE OFF. Double-click each dial after moving it to verify
   default reset. Hover to inspect tooltips; Tab and arrow keys should adjust dials.
4. Drag each dial while playing audio. Check the native control and FL parameter
   value agree. Automate each existing parameter from FL and confirm the editor
   follows; REVERSE and FREEZE should visibly change state.
5. Compare the GHOST dial at 0/50/100%. The arc/value should remain readable while
   glow increases. FREEZE ON should be more prominent than REVERSE ON.
6. Set GHOST 100% for comparison with the verified DSP, exercise MEMORY, DECAY,
   DRIFT, BLUR, REVERSE and FREEZE, then verify MIX zero remains dry. No new sonic
   behavior is intended. The macro retains its existing wet-mute behavior at zero.
7. Save with non-default controls, close/reopen the editor and project, and verify
   displayed values/toggles restore. Frozen audio itself still does not survive
   project reload; switch OFF and recapture before expecting a frozen texture.

At the static foundation stage, visual limits included no animated smoke or
signal visualizer. Remaining limits include fixed-size layout with no user zoom
control; whole-percent central readouts despite continuous
underlying values; system-font appearance may vary; cached background/logo are
optimized for up to 2x rendering. Actual FL Studio window placement, wrapper
scaling, and interaction on a physical HiDPI monitor remain manual host checks.

Final static-UI verification: Release VST3 and all three test executables built
successfully with no remaining new warnings. Complete CTest passed **3/3 tests,
zero failures**: DSP 317.25 seconds, processor/state 2.88 seconds, UI 0.60 seconds
(317.25 seconds wall time with parallel test execution). All prior DSP tests at
44.1/48/88.2/96 kHz passed unchanged. Protected-source SHA-256 verification matched
all 18 baseline files; embedded logo SHA-256 also matches the original image.


## Animated ghost-energy visualizer

`GhostVisualizer` is a separate message-thread component, placed at (172,97) with
size 476 x 236, behind every control. It owns its JUCE Timer, seven cached APVTS
pointers, smoothed visual state, fixed particle storage, and preallocated paths.
The static background, logo, controls, layout, parameter definitions/defaults,
DSP algorithms and 1400-second host tail are unchanged.

### Metering without changing audio

Only `PluginProcessor.h/.cpp` changed outside UI/test/build/documentation code:

- Read-only stereo input and final-output block peaks are calculated before and
  after the existing engine call, respectively. Samples are never written by
  metering and all parameter setters retain their existing order.
- Display peaks clamp to 0-1, ignore non-finite input, rise immediately, and release
  with `envelope = max(blockPeak, previous * exp(-blockSamples/(sampleRate*0.25)))`.
  Values below 1e-8 become zero to avoid denormal envelope tails.
- Two relaxed atomic floats publish those envelopes. An atomic unsigned heartbeat
  advances on each nonempty processed block, or meter reset. Compile-time checks
  require lock-free atomics. Reset/release/prepare clear meter values.
- No heap allocation, locks, audio copies, FFT, editor dependency or audio-thread
  animation was added. The extra work is two linear read-only sample scans, one
  exponential per nonempty block, and a few scalar/atomic operations.

**The second meter is final output, not isolated wet audio.** An isolated wet
measurement would require modifying the protected engine. Output therefore
includes dry material according to MIX; input also excites the visualizer. With
MIX zero, muted input and a frozen internal texture, the visualizer cannot measure
that inaudible wet texture directly. This limitation is deliberate and disclosed.

`visualizer-source-baseline.json` records the pre-change hashes. All 16 DSP files
remain byte-identical; the only changed protected-source entries are the two
processor files with the metering additions above. The logo asset and its drawing
code are unchanged.

### Motion, response and smoothing

The Timer targets **30 FPS**. Actual elapsed time drives animation, capped at
100 ms per update after message-thread stalls. Hidden editors skip geometry and
repainting while their cheap timer remains registered. Destruction stops the timer;
FREEZE never stops it. The editor is not required for audio or meter processing.

There are **12 wisps**, each with a primary path and two echoes, plus **12 fixed
particles**. Geometry is entirely deterministic: sine/cosine, fixed golden-angle
phase offsets and integer harmonics. No random-device calls or per-frame particle
collection allocation occur. Phase wraps continuously modulo 2*pi for long runs.
Each wisp path uses two cubic segments, with wandering curvature and radius.

Controls are normalized 0-1 and read only; no visual interaction changes DSP:

| Control | Visual behavior |
| --- | --- |
| GHOST | Intensity `0.003 + g^1.6*(0.035 + 0.28*energy)`; zero retains faint atmosphere |
| MEMORY | Adds up to 14 pixels to wisp radius, subtly opening the field |
| DECAY | Extends envelope release and echo separation/opacity |
| BLUR | Widens translucent strokes and reduces the sharp core's opacity |
| DRIFT | Adds radial/lateral wandering and slightly increases orbital speed |
| REVERSE | Eases circulation from clockwise to counterclockwise, through zero speed |
| FREEZE | Eases speed toward 18% of normal, shifts toward the existing FREEZE accent, and fades particles into diamond-like marks |

All smoothing uses exponential approach (`1-exp(-dt/tau)`), not hard state jumps.
Time constants are 180 ms for GHOST, 400 ms for MEMORY/DRIFT/direction, 300 ms for
DECAY/BLUR, and 450 ms for FREEZE. A time constant is not a fixed completion time.

The visual audio target is `sqrt(max(inputMeter, 0.85*outputMeter))`. It attacks
with a 120 ms time constant and releases with `0.25 + 1.75*decay + 0.8*freeze`
seconds. Brightness, trail extent, particle size and radial displacement respond
to this energy. If the heartbeat is unchanged for 150 ms (host suspended/stopped),
the target becomes zero and releases smoothly instead of holding a stale peak.

Angular speed is `(0.38 + 0.16*drift) * direction * (1 - 0.82*freeze)` radians per
second. REVERSE changes direction smoothly rather than relocating the paths.
A 97-pixel-radius circular exclusion keeps the dial face/readout/pointer clear;
the component's outer bounds keep the logo and lower controls outside animation.
All colors derive from GhostTheme mint/pale/FREEZE accents.

### Rendering cost

Each of 36 short paths receives three translucent strokes: at most **108 strokes
per frame**, plus the 12 small particle glows/marks. Path storage is reserved once;
frame updates clear and reuse it. No large offscreen buffer, Gaussian blur, FFT,
OpenGL or new rendering thread was introduced. The existing static background
cache remains unchanged. Paint performs no I/O, explicit locking or blocking.
JUCE may allocate internally while rasterizing strokes; there are no per-frame
particle collections or large temporary buffers created by this component.
No host CPU percentage benchmark is claimed. Busy message threads may deliver
less than 30 FPS; animation never controls audio scheduling.

### Visualizer checks and previews

The existing DSP/state tests remain intact. Additional UI-suite checks compare
the metered processor with the unchanged standalone engine using `memcmp` for
both output channels at 44.1/48/88.2/96 kHz and block sizes 32/64/256/512/1024.
These cover active REVERSE/FREEZE and macro changes, plus non-finite input and
empty blocks. All 20 rate/block combinations produce bit-identical audio. Meter
values remain finite and reset correctly; audio runs without any editor.

Other checks cover valid cached parameter bindings, GHOST zero/full intensity,
smooth direction reversal, FREEZE slowdown without stopping, parameter response,
stale-meter fade, finite phase, actual JUCE timer callback delivery (including
FREEZE), explicit timer stop/start, and ten editor open/close/destruction cycles.
All previous attachment/keyboard/state-reopen checks are preserved.

Previews render real editor geometry after deterministic animation steps driven
by synthetic audio, not an unrelated mockup:

- `build/UI-Previews/GHOST-default.png`: original defaults with input activity.
- `build/UI-Previews/GHOST-active.png`: GHOST/MIX 100%, forward moving energy.
- `build/UI-Previews/GHOST-reverse.png`: reversed circulation at a representative phase.
- `build/UI-Previews/GHOST-freeze.png`: slowed, pale crystalline energy with FREEZE/REVERSE ON.
- Existing zero-intensity and 2x active previews are also regenerated.

Still images verify composition, not continuous animation. Timer/state tests
verify temporal behavior numerically; perceived fluidity and host repaint cost
still need the live FL Studio check below. The complete run is retained in
`visualizer-verification.log`.

### FL Studio animated visual verification

1. Close FL Studio, replace the entire installed GHOST.vst3 bundle with
   `build/GHOST_artefacts/Release/VST3/GHOST.vst3`, reopen, and rescan if needed.
2. Open GHOST, play a vocal or drum phrase, and watch the area around the macro.
   Trails/particles should move without obscuring the percentage, pointer, logo,
   labels or smaller controls. Try dragging the macro through the halo area;
   the decorative component must never steal mouse clicks.
3. Compare GHOST 0/25/50/75/100%. At zero only faint animated atmosphere remains;
   increasing it makes the audio-reactive field brighter and larger. Compare
   loud hits with quiet material for smooth pulses rather than flicker.
4. Raise DRIFT for wandering, BLUR for wider/softer forms, DECAY for lingering
   visual energy, and MEMORY for a modest outward radius change. Audio behavior
   should remain the previously verified behavior of those controls.
5. Toggle REVERSE while watching identifiable particles: circulation should slow,
   then change direction smoothly. Toggle FREEZE: motion should become suspended
   but remain subtly alive, pale accents/crystalline marks should increase, and
   the controls must remain responsive.
6. Stop playback or let FL suspend processing. The last meter value should not
   keep bright energy stuck indefinitely. With processing still active, actual
   audible frozen output may legitimately continue driving the output meter.
7. Close the editor while audio plays, reopen it repeatedly, and confirm audio
   and parameter values are unaffected. Save/reload to check existing parameter
   restoration; frozen audio remains intentionally unserialized.
8. Check normal display scaling and HiDPI, rapid control automation, and overall
   UI responsiveness while the project is busy. No strict live-host FPS/CPU
   guarantee is inferred from offscreen screenshots or unit tests.

Known limits: final-output rather than isolated-wet metering; display peaks are
clamped to 0-1; visual motion is procedural rather than an audio waveform/spectrum;
no animation state is serialized; hidden editors pause geometry updates; host
message-thread load can reduce frame rate. The existing fixed-size layout, frozen
capture reload limitation, and unchanged host-tail declaration still apply.

Final animated-visualizer verification: Release VST3 and all three test executables
built successfully with no new warnings. Complete CTest passed **3/3, zero
failures**: DSP 368.04 seconds, processor/state 3.43 seconds, UI 6.82 seconds;
total wall time 368.04 seconds. The metering comparison passed all 20 sample-rate/
block-size combinations bit-for-bit. All 16 DSP source files remain unchanged.
See `visualizer-verification.log` for the complete run. Live FL Studio animation
and CPU observations remain manual checks, not claimed test results.


## Factory presets

Ten immutable factory presets are defined in `Source/Presets/GhostPresetManager.h/.cpp`.
They set the existing eight APVTS parameters using normalized values and host-notified
begin/end gestures. They introduce no DSP state, engine reset, history clear, new
parameter, disk preset format, or audio capture serialization. Existing smoothing
and transitions handle parameter changes. All starting values were retained exactly.

| Preset | GHOST | MEMORY | DECAY | BLUR | DRIFT | MIX | REVERSE | FREEZE | Purpose |
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

Numeric values are percentages. Every load turns FREEZE off first, including when
loading Frozen Choir or changing from an active capture. No preset contains frozen
audio. Feed suitable material into Frozen Choir and manually enable FREEZE afterward.
No safety limits or DSP formulas were changed.

### Selector and state

The dark/green/pale selector is beside the original logo, with understated previous
and next buttons, keyboard focus, and accessible titles/tooltips. The small version
label moves below it. All existing control bounds, logo drawing and the visualizer
architecture remain unchanged. Previous/next wrap; from Custom, next selects Whisper
and previous selects Possessed. The popup supports JUCE keyboard navigation.

The manager derives the displayed name by comparing live APVTS values against the
factory data (0.0001 percentage-point tolerance). A separate 10 Hz editor timer
refreshes the label without listening or allocating on the audio thread. Nonmatching
values show Custom, including startup defaults. Returning exactly to a factory
configuration shows that preset name again, regardless of how those values were set.
There is no last-loaded-name metadata. Manual edits and automation never reapply a
preset. Existing project serialization retains the actual APVTS values; loading a
project never looks up a factory name to overwrite them. The processor source files
are unchanged. The host program API remains unchanged; these are in-plugin presets,
not a new host program bank or external .vstpreset files.

Preset application is a message-thread operation with individual parameter gestures,
not a multi-parameter atomic transaction. An audio block may see intermediate settings
while loading; existing smoothing remains active. Host automation can subsequently
override loaded values. The UI's label may lag an edit by about 100 ms. Factory data
is read-only; user preset saving/renaming is outside this stage.

### Verification and preview

`Tests/GhostPresetTests.cpp` runs within GHOST_UITests and checks all names/values,
ranges, defaults, FREEZE release, REVERSE presets, empty Frozen Choir output,
attachments, selector navigation, Custom detection, editor cycles, and project
roundtrips before/after editing. At all four rates, 18 seconds of stereo processing
includes 11 seconds populating history followed by rapid switches every 13 blocks
(256 samples). Output is bit-identical to a second processor given the same parameters
manually. Maximum observed peaks for this stimulus:

| Rate | Peak | dBFS |
| --- | ---: | ---: |
| 44.1 kHz | 0.435852 | -7.21322 |
| 48 kHz | 0.434567 | -7.23887 |
| 88.2 kHz | 0.407498 | -7.79748 |
| 96 kHz | 0.407159 | -7.80471 |

No non-finite samples or excessive peaks occurred. These are measured test-stimulus
peaks, not a universal output ceiling. Existing stress tests still cover the DSP.
Timer checks now poll for actual delivery for up to two seconds instead of assuming
Windows schedules a callback within a single 60 ms sleep; the visualizer itself is
unchanged. `preset-source-baseline.json` records byte hashes of all 16 DSP files,
the two processor files and the two visualizer files, all unchanged by this stage.

New preview: `build/UI-Previews/GHOST-presets.png` (Haunted Vocal). Previous preview
files are preserved; existing UI checks still run, and new snapshots are rendered
but do not overwrite existing prior-stage artifacts.

### FL Studio preset verification

1. Close FL Studio, replace the installed full GHOST.vst3 bundle with
   `build/GHOST_artefacts/Release/VST3/GHOST.vst3`, then reopen and rescan if needed.
2. Add a new instance on an active mixer insert. Confirm Custom and the original
   defaults: GHOST 35, MEMORY 40, DECAY 40, BLUR 20, DRIFT 10, MIX 35, both toggles off.
3. Play vocals/drums/guitar. Select Haunted Vocal and check 70/45/48/35/22/38,
   REVERSE/FREEZE off. Check that all controls and visual energy respond.
4. Cycle all ten presets with the popup and arrows. Check wrapping, keyboard focus,
   readable labels, and absence of controls obscuring the logo or main knob.
5. Load Haunted Vocal, change BLUR to 50, observe Custom, save the project, close
   and reopen it. Confirm BLUR stays 50 and the other saved values restore.
6. Feed sustained vocal material into Frozen Choir, then enable FREEZE. Select
   another factory preset and confirm FREEZE turns off. A fresh Frozen Choir load
   has no stored audio; captured audio still does not survive project reload.
7. Populate history for at least 10 seconds, then rapidly alternate Possessed,
   Whisper, Deep Space and Reverse Spirit during playback. Check smooth transitions,
   finite meters, no unexpected loud spikes, and continued visualizer responsiveness.
8. Close/reopen the editor during playback and confirm continued audio and correct
   selector/parameter values. Check normal and HiDPI display scaling.

Live FL Studio audition and host-specific gesture/automation behavior remain manual
checks. No installer, startup-default change, or host-tail improvement is included.

Final preset verification: Release VST3 and all test executables build without new
warnings. Complete regression execution passed DSP (286.58 s) and processor/state
(2.83 s). The initial UI run exposed the existing 60 ms timer scheduling assumption;
after the bounded callback-wait correction, CTest --rerun-failed passed the UI suite
including every new preset check (7.62 s). All three suites now pass. Preserve both
`preset-verification.log` (full run, including the original timer failure) and
`preset-ui-verification.log` (successful corrected UI rerun) as the audit record.
