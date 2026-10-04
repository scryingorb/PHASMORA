#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

// Audio-thread-only stereo return. It stores one frame, never audio history.
// The engine writes dry + returned ghosts; the output MIX is outside this loop.
class GhostFeedback
{
public:
    void prepare(double sampleRate, double leftVoiceGainSum, double rightVoiceGainSum) noexcept;
    void reset() noexcept;
    void setDecay(float normalizedDecay) noexcept;
    void getReturn(float& left, float& right) noexcept;
    void capture(float wetLeft, float wetRight) noexcept;

    static constexpr double maximumCoefficient = 0.95;

private:
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> coefficient { 0.608 };
    std::array<double, 2> inverseGainSum { 1.0, 1.0 };
    std::array<float, 2> previousWet { 0.0f, 0.0f };
};
