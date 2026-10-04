#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

// Target-derived modulation uses each existing module's smoother. A separate
// 150 ms normalized ramp drives audible gain and pan continuously per sample.
class GhostMacro
{
public:
    struct Scales { double level, width, drift, blur, decay; };
    bool setAmount(float normalized) noexcept;
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    Scales next() noexcept;
    Scales targets() const noexcept;
    static Scales map(double normalized) noexcept;
private:
    // Standalone engine defaults to unity for legacy DSP callers. The processor
    // explicitly forwards the production APVTS default (65%) before prepare.
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> amount { 1.0 };
};
