#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

// Per-voice tone softening followed by a short stereo diffuser. Audio-thread
// only: call beginFrame once, filterVoice once per voice, then diffuse once.
class GhostBlur
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void setAmount(float normalizedBlur) noexcept;
    void beginFrame() noexcept;
    void filterVoice(size_t voice, float& left, float& right) noexcept;
    void diffuse(float& left, float& right) noexcept;

private:
    struct AllPass
    {
        void prepare(double sampleRate, double milliseconds);
        void reset() noexcept;
        float process(float input) noexcept;
        std::vector<float> delay;
        size_t position = 0;
    };

    double rate = 0.0;
    float amount = 0.0f;
    std::array<std::array<float, 2>, 4> filterState {};
    std::array<std::array<AllPass, 3>, 2> diffusers;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> pole { 0.0 };
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> filterBlend { 0.0 };
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> diffusionBlend { 0.0 };
    float framePole = 0.0f;
    float frameFilterBlend = 0.0f;
    float frameDiffusionBlend = 0.0f;
    void updateTargets() noexcept;
};
