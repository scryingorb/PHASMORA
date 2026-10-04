#pragma once
#include "GhostBuffer.h"
#include "GhostReverse.h"
#include <juce_audio_basics/juce_audio_basics.h>

// One stereo voice, sharing the engine's history rather than owning a delay line.
// All methods are audio-thread-only (prepare/reset must not run concurrently).
class GhostVoice
{
public:
    void prepare(double sampleRate, double maxDelaySeconds, double initialDelaySeconds,
                 float gainDecibels, float pan, double lfoRateHz = 0.07,
                 double initialPhaseCycles = 0.0, GhostReverseSettings reverseSettings = {}) noexcept;
    void reset() noexcept;
    void setDelaySeconds(double seconds) noexcept;
    void setDrift(float normalizedDrift) noexcept;
    void setStereoWidth(double normalizedWidth) noexcept;
    void setReverse(bool enabled) noexcept { reverseBlend.setTargetValue(enabled ? 1.0 : 0.0); }
    void processSample(const GhostBuffer& history, float& wetLeft, float& wetRight) noexcept;

    // Audio-thread diagnostic accessors for DSP tests; not UI/thread-safe state.
    double getPhaseCycles() const noexcept { return phase; }
    double getReadDelaySamples() const noexcept { return readDelay; }
    double getDestinationReadDelaySamples() const noexcept { return destinationReadDelay; }
    double getDepthSamples() const noexcept { return depth.getCurrentValue(); }

private:
    double rate = 0.0;
    double maxDelaySamples = 0.0;
    double currentDelay = 0.0;
    double destinationDelay = 0.0;
    double requestedDelay = 0.0;
    int fadeLength = 1;
    int fadePosition = 0;
    bool fading = false;
    double basePan = 0.0, linearGain = 1.0, stereoWidth = 1.0;
    float leftGain = 0.0f;
    float rightGain = 0.0f;
    double lfoHz = 0.07;
    double initialPhase = 0.0;
    double phase = 0.0;
    double phaseStep = 0.0;
    float driftAmount = 0.0f;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> depth { 0.0 };
    double readDelay = 0.0;
    double destinationReadDelay = 0.0;
    GhostReverse reverse;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> reverseBlend { 0.0 };
    double modulatedDelay(double baseDelay, double amplitude, double sine) const noexcept;
};
