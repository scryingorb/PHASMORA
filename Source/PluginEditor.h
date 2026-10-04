#pragma once
#include "PluginProcessor.h"
#include "UI/GhostLookAndFeel.h"
#include "UI/GhostKnob.h"
#include "UI/GhostVisualizer.h"
#include "Presets/GhostPresetManager.h"

class GhostAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GhostAudioProcessorEditor(GhostAudioProcessor&);
    ~GhostAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void selectPreset(int);
    void stepPreset(int);
    juce::AudioProcessorValueTreeState& state;
    GhostLookAndFeel lookAndFeel;
    juce::ComboBox presetSelector;
    juce::TextButton previousPreset { "<" }, nextPreset { ">" };
    juce::Image logo, backdrop;
    GhostVisualizer visualizer;
    GhostKnob ghost, memory, decay, blur, drift, mix;
    juce::ToggleButton reverse { "REVERSE" }, freeze { "FREEZE" };
    juce::AudioProcessorValueTreeState::ButtonAttachment reverseAttachment, freezeAttachment;
    juce::TooltipWindow tooltips { this, 650 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GhostAudioProcessorEditor)
};
