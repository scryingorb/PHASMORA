#include "GhostEngine.h"
#include <cmath>

namespace
{
struct VoiceSettings { double depthRatio; float gainDb; float pan; double lfoHz; double phase;
                       double reverseMin; double reverseMax; };
constexpr std::array<VoiceSettings, 4> settings {{
    { 0.20, -12.0f, -0.25f, 0.07, 0.00, 0.15, 0.50 },
    { 0.39, -15.0f,  0.25f, 0.11, 0.23, 0.25, 0.80 },
    { 0.64, -18.0f, -0.55f, 0.17, 0.51, 0.40, 1.20 },
    { 0.93, -21.0f,  0.55f, 0.23, 0.79, 0.60, 1.80 }
}};

float validInput(float value) noexcept
{
    // Invalid/absurd upstream samples must not poison ten seconds of history or
    // escape via the dry path. This fault guard rejects > +18 dBFS, not a normal
    // audio limiter; valid input (including reasonable floating-point overs) is
    // unchanged. Loop stability comes from normalized gain, not this guard.
    return std::isfinite(value) && std::abs(value) <= 8.0f ? value : 0.0f;
}
}

double GhostEngine::memoryDepthSeconds(float normalizedMemory) noexcept
{
    // Piecewise-linear nonlinear knob curve. At 0/25/50/75/100% the maximum
    // depth is 0.5/1.5/4/7/10 seconds. Lower knob positions have finer control.
    constexpr std::array<double, 5> depths { 0.5, 1.5, 4.0, 7.0, 10.0 };
    const auto x = static_cast<double>(normalizedMemory) * 4.0;
    const auto segment = juce::jmin(3, static_cast<int>(x));
    const auto fraction = x - static_cast<double>(segment);
    const auto index = static_cast<size_t>(segment);
    return depths[index] + fraction * (depths[index + 1] - depths[index]);
}

void GhostEngine::setMemory(float normalizedMemory) noexcept
{
    if (!std::isfinite(normalizedMemory))
        return;
    const auto bounded = juce::jlimit(0.0f, 1.0f, normalizedMemory);
    if (bounded == memoryAmount)
        return;
    memoryAmount = bounded;
    updateVoiceDelays();
}

void GhostEngine::updateVoiceDelays() noexcept
{
    if (preparedChannels == 0) return;
    const auto liveDepth = memoryDepthSeconds(memoryAmount);
    const auto frozenDepth = std::min(liveDepth, frozen.capturedSamples() / preparedRate);
    for (size_t i = 0; i < voices.size(); ++i)
    {
        voices[i].setDelaySeconds((freezeActive && !freezeReleasing ? frozenDepth : liveDepth) * settings[i].depthRatio);
        frozenVoices[i].setDelaySeconds(frozenDepth * settings[i].depthRatio);
    }
}

void GhostEngine::prepare(double sampleRate, int samplesPerBlock, int numChannels)
{
    juce::ignoreUnused(samplesPerBlock); // Storage is independent of block size.
    macro.prepare(sampleRate);
    memory.prepare(sampleRate, maximumMemorySeconds, numChannels);
    memory.prepareCapture();
    frozen.prepare(sampleRate, numChannels);
    freezeActive = freezeReleasing = false;
    freezeBlend.reset(sampleRate, 0.15);
    frozenReturnGain.reset(sampleRate, 0.1);
    frozenReturnGain.setCurrentAndTargetValue(0.0);
    freezeBlend.setCurrentAndTargetValue(freezeRequested ? 1.0 : 0.0);
    preparedChannels = numChannels;
    preparedRate = sampleRate;
    for (size_t i = 0; i < voices.size(); ++i)
    {
        voices[i].setDrift(static_cast<float>(driftAmount * macro.targets().drift));
        voices[i].prepare(sampleRate, maximumMemorySeconds,
                          memoryDepthSeconds(memoryAmount) * settings[i].depthRatio,
                          settings[i].gainDb, settings[i].pan, settings[i].lfoHz, settings[i].phase,
                          { settings[i].depthRatio, settings[i].reverseMin, settings[i].reverseMax });
    }
    frozenVoices = voices;
    double leftSum = 0.0, rightSum = 0.0;
    for (const auto& voice : settings)
    {
        const auto gain = std::pow(10.0, static_cast<double>(voice.gainDb) / 20.0);
        const auto angle = (static_cast<double>(voice.pan) + 1.0) * juce::MathConstants<double>::pi / 4.0;
        leftSum += gain * std::cos(angle);
        rightSum += gain * std::sin(angle);
    }
    updateMacroTargets();
    feedback.prepare(sampleRate, leftSum, rightSum);
    blur.prepare(sampleRate);
    mix.reset(sampleRate, 0.05); // 50 ms; advance once per frame, not per channel.
    mix.setCurrentAndTargetValue(mix.getTargetValue());
}

void GhostEngine::reset()
{
    macro.reset();
    updateMacroTargets();
    memory.reset();
    frozen.reset();
    frozenReturnGain.setCurrentAndTargetValue(0.0);
    freezeActive = freezeReleasing = false;
    freezeBlend.setCurrentAndTargetValue(freezeRequested ? 1.0 : 0.0);
    for (auto& voice : frozenVoices) voice.reset();
    feedback.reset();
    blur.reset();
    for (auto& voice : voices)
        voice.reset();
    mix.setCurrentAndTargetValue(mix.getTargetValue());
    updateVoiceDelays();
}

void GhostEngine::setMix(float normalizedMix) noexcept
{
    if (std::isfinite(normalizedMix))
        mix.setTargetValue(juce::jlimit(0.0f, 1.0f, normalizedMix));
}

void GhostEngine::setReverse(bool enabled) noexcept
{
    for (auto& voice : voices)
        voice.setReverse(enabled);
    for (auto& voice : frozenVoices) voice.setReverse(enabled);
}

void GhostEngine::setDrift(float normalizedDrift) noexcept
{
    if (!std::isfinite(normalizedDrift))
        return;
    const auto bounded = juce::jlimit(0.0f, 1.0f, normalizedDrift);
    if (bounded == driftAmount)
        return;
    driftAmount = bounded;
    updateMacroTargets();
}

void GhostEngine::setGhost(float normalizedGhost) noexcept
{
    if (macro.setAmount(normalizedGhost)) updateMacroTargets();
}
void GhostEngine::setDecay(float normalizedDecay) noexcept
{
    if (!std::isfinite(normalizedDecay)) return;
    const auto bounded = juce::jlimit(0.0f, 1.0f, normalizedDecay);
    if (bounded == decayAmount) return;
    decayAmount = bounded;
    updateMacroTargets();
}
void GhostEngine::setBlur(float normalizedBlur) noexcept
{
    if (!std::isfinite(normalizedBlur)) return;
    const auto bounded = juce::jlimit(0.0f, 1.0f, normalizedBlur);
    if (bounded == blurAmount) return;
    blurAmount = bounded;
    updateMacroTargets();
}
void GhostEngine::updateMacroTargets() noexcept
{
    const auto scales = macro.targets();
    feedback.setDecay(static_cast<float>(decayAmount * scales.decay));
    blur.setAmount(static_cast<float>(blurAmount * scales.blur));
    for (auto& voice : voices) voice.setDrift(static_cast<float>(driftAmount * scales.drift));
    for (auto& voice : frozenVoices) voice.setDrift(static_cast<float>(driftAmount * scales.drift));
}

void GhostEngine::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (preparedChannels != 2 || buffer.getNumChannels() != preparedChannels)
        return;

    juce::ScopedNoDenormals noDenormals;
    auto* const* samples = buffer.getArrayOfWritePointers();
    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        const auto macroFrame = macro.next();
        // Capture BEFORE accepting this frame's input. Rapid requests coalesce
        // until an OFF fade completes; the next ON captures the then-current live history.
        if (freezeRequested && !freezeActive)
        {
            frozen.capture(memory);
            frozenReturnGain.setCurrentAndTargetValue(0.0);
            frozenVoices = voices; // Fixed state only; preserves all LFO phases.
            freezeActive = true;
            freezeReleasing = false;
            updateVoiceDelays();
        }
        // Finish an OFF handover before accepting a rapid re-capture request.
        // This prevents an audible mid-fade reader relocation.
        if (freezeActive && !freezeRequested && !freezeReleasing)
        {
            freezeReleasing = true;
            updateVoiceDelays();
        }
        freezeBlend.setTargetValue(freezeActive && !freezeReleasing ? 1.0 : 0.0);
        const auto sourceBlend = static_cast<float>(freezeBlend.getNextValue());
        if (freezeReleasing && sourceBlend == 0.0f && freezeActive)
        {
            memory.endCapture();
            freezeActive = freezeReleasing = false;
        }
        const auto blend = static_cast<float>(mix.getNextValue());
        const auto angle = blend * juce::MathConstants<float>::halfPi;
        // Preserve the equal-power MIX crossfade and exact endpoints.
        // Explicit endpoints avoid a tiny residual dry signal from cos(pi/2).
        const auto dryGain = blend >= 1.0f ? 0.0f : std::cos(angle);
        const auto wetGain = blend <= 0.0f ? 0.0f : std::sin(angle);

        const auto dryLeft = validInput(samples[0][frame]);
        const auto dryRight = validInput(samples[1][frame]);
        float returnLeft = 0.0f, returnRight = 0.0f;
        feedback.getReturn(returnLeft, returnRight);
        // One-frame causal return preserves the original write-then-read timing.
        // Write each dry sample exactly once, plus only processed ghost content.
        // The explicit frame of feedback state also makes block size irrelevant.
        memory.writeSample(0, dryLeft + returnLeft);
        memory.writeSample(1, dryRight + returnRight);
        if (freezeActive)
        {
            // Let the captured source settle before ramping new regeneration
            // in over 100 ms. Release ramps it out; original capture is immutable.
            frozenReturnGain.setTargetValue(!freezeReleasing && sourceBlend == 1.0f ? 1.0 : 0.0);
            const auto regeneration = static_cast<float>(frozenReturnGain.getNextValue());
            frozen.writeFrame(regeneration * returnLeft, regeneration * returnRight);
        }
        blur.beginFrame();
        float wetLeft = 0.0f, wetRight = 0.0f;
        for (size_t voice = 0; voice < voices.size(); ++voice)
        {
            voices[voice].setStereoWidth(macroFrame.width);
            float voiceLeft = 0.0f, voiceRight = 0.0f;
            // The outgoing reader inherits the exact captured history on ON.
            // Keep it on that advancing, capture-only history during the hold:
            // otherwise new input in the live leg of the fade could leak into
            // downstream BLUR state and subsequently into frozen feedback.
            // On release, real live history returns under the 150 ms fade.
            const auto& outgoingHistory = freezeActive && !freezeReleasing ? frozen.history() : memory;
            voices[voice].processSample(outgoingHistory, voiceLeft, voiceRight);
            if (freezeActive)
            {
                frozenVoices[voice].setStereoWidth(macroFrame.width);
                float frozenLeft = 0.0f, frozenRight = 0.0f;
                frozenVoices[voice].processSample(frozen.history(), frozenLeft, frozenRight);
                if (sourceBlend == 1.0f)
                {
                    voiceLeft = frozenLeft;
                    voiceRight = frozenRight;
                }
                else
                {
                    voiceLeft += sourceBlend * (frozenLeft - voiceLeft);
                    voiceRight += sourceBlend * (frozenRight - voiceRight);
                }
            }
            blur.filterVoice(voice, voiceLeft, voiceRight);
            wetLeft += voiceLeft;
            wetRight += voiceRight;
        }
        // Both listening and regeneration inherit filtering and short diffusion.
        // No extra DRIFT, long-delay movement, gain compensation or output limiter.
        blur.diffuse(wetLeft, wetRight);
        // Gate AFTER diffusion so old filter/diffuser tails cannot leak through
        // at macro zero. Both listening and feedback receive this scaled field.
        // Keep unity arithmetic unchanged for pre-macro regression.
        if (macroFrame.level != 1.0)
        {
            wetLeft *= static_cast<float>(macroFrame.level);
            wetRight *= static_cast<float>(macroFrame.level);
        }
        feedback.capture(wetLeft, wetRight);
        samples[0][frame] = dryGain * dryLeft + wetGain * wetLeft;
        samples[1][frame] = dryGain * dryRight + wetGain * wetRight;
    }
}
