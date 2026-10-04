#include "GhostPresetManager.h"
#include <cmath>

const std::array<GhostPresetManager::Preset, 10>& GhostPresetManager::presets()
{
    static const std::array<Preset, 10> data {{
        { "Whisper",       {35,25,20,15,12,22,0,0} },
        { "Haunted Vocal", {70,45,48,35,22,38,0,0} },
        { "Lost Memory",   {75,80,60,55,28,45,0,0} },
        { "Ectoplasm",     {85,55,72,70,40,50,0,0} },
        { "Deep Space",    {80,90,85,82,35,55,0,0} },
        { "Frozen Choir",  {78,65,68,72,30,45,0,0} },
        { "Reverse Spirit",{82,60,50,42,25,45,1,0} },
        { "Ghost Guitar",  {58,38,38,24,16,32,0,0} },
        { "Dream State",   {72,68,78,88,32,52,0,0} },
        { "Possessed",     {100,82,88,68,72,62,1,0} }
    }};
    return data;
}

bool GhostPresetManager::load(juce::AudioProcessorValueTreeState& state, int index)
{
    if (index < 0 || index >= static_cast<int>(presets().size())) return false;
    std::array<juce::RangedAudioParameter*, 8> parameters {};
    for (size_t i = 0; i < ids.size(); ++i)
        if ((parameters[i] = state.getParameter(ids[i])) == nullptr) return false;
    // FREEZE off first; never serialize or manufacture an audio capture.
    // Per-parameter host gestures preserve automation and attachment behavior.
    for (int step = 0; step < 8; ++step)
    {
        const auto i = static_cast<size_t>((step + 7) % 8);
        auto& parameter = *parameters[i];
        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost(parameter.convertTo0to1(presets()[static_cast<size_t>(index)].values[i]));
        parameter.endChangeGesture();
    }
    return true;
}

int GhostPresetManager::matchingPreset(const juce::AudioProcessorValueTreeState& state)
{
    for (size_t index = 0; index < presets().size(); ++index)
    {
        bool matches = true;
        for (size_t i = 0; i < ids.size(); ++i)
        {
            const auto* value = state.getRawParameterValue(ids[i]);
            if (value == nullptr || !(std::abs(value->load(std::memory_order_relaxed)
                                      - presets()[index].values[i]) < 0.0001f))
            { matches = false; break; }
        }
        if (matches) return static_cast<int>(index);
    }
    return -1;
}
