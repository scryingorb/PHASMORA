#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Standard JUCE slider interaction/accessibility, entirely custom visual paint.
// Attachment declared last so it detaches before the slider is destroyed.
class GhostKnob : public juce::Component
{
public:
    GhostKnob(juce::AudioProcessorValueTreeState&, const juce::String& id,
              const juce::String& name, const juce::String& tooltip, bool macro = false);
    void resized() override;
private:
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GhostKnob)
};
