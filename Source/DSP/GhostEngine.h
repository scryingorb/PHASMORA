#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "GhostBuffer.h"
#include "GhostVoice.h"
#include "GhostFeedback.h"
#include "GhostBlur.h"
#include "GhostFreeze.h"
#include "GhostMacro.h"
#include <array>

class GhostEngine
{
public:
    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    void processBlock(juce::AudioBuffer<float>& buffer);
    void setMix(float normalizedMix) noexcept;
    void setMemory(float normalizedMemory) noexcept;
    void setDecay(float normalizedDecay) noexcept;
    void setGhost(float normalizedGhost) noexcept;
    void setDrift(float normalizedDrift) noexcept;
    void setBlur(float normalizedBlur) noexcept;
    void setReverse(bool enabled) noexcept;
    void setFreeze(bool enabled) noexcept { freezeRequested = enabled; }

    static constexpr double maximumMemorySeconds = 10.0;
    // Legacy mathematical bound, not host-tail reporting (see processor metadata).
    // Conservative -60 dB bound: 0.95^140 < 0.001, each history traversal < 10s.
    static constexpr double maximumTailSeconds = 1400.0;

private:
    GhostMacro macro;
    float decayAmount = 0.40f, blurAmount = 0.0f;
    void updateMacroTargets() noexcept;
    GhostBuffer memory;
    GhostFreeze frozen;
    std::array<GhostVoice, 4> frozenVoices;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> freezeBlend { 0.0 };
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> frozenReturnGain { 0.0 };
    bool freezeRequested = false, freezeActive = false, freezeReleasing = false;
    std::array<GhostVoice, 4> voices;
    GhostFeedback feedback;
    GhostBlur blur;
    // Double precision prevents accumulated rounding error across long ramps.
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> mix { 0.35 };
    float memoryAmount = 0.40f;
    float driftAmount = 0.10f;
    int preparedChannels = 0;
    double preparedRate = 0.0;
    void updateVoiceDelays() noexcept;
    static double memoryDepthSeconds(float normalizedMemory) noexcept;
};
