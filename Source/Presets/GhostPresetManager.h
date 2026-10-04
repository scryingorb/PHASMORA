#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

// Factory data is immutable. Live sound/state always belongs to the APVTS.
class GhostPresetManager
{
public:
    struct Preset { const char* name; std::array<float, 8> values; };
    inline static constexpr std::array<const char*, 8> ids {
        "ghost", "memory", "decay", "blur", "drift", "mix", "reverse", "freeze" };
    static const std::array<Preset, 10>& presets();
    // Message-thread operation: notify host and existing attachments, never reset DSP.
    static bool load(juce::AudioProcessorValueTreeState&, int index);
    // Derived from current values; no stale preset label to serialize.
    static int matchingPreset(const juce::AudioProcessorValueTreeState&);
};
