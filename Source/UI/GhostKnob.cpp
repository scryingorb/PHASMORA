#include "GhostKnob.h"

GhostKnob::GhostKnob(juce::AudioProcessorValueTreeState& state, const juce::String& id,
                     const juce::String& name, const juce::String& tooltip, bool macro)
    : attachment(state, id, slider)
{
    slider.setComponentID(id);
    slider.setName(name);
    slider.setTitle(name);
    slider.setDescription(tooltip);
    slider.setTooltip(tooltip);
    slider.getProperties().set("ghostMacro", macro);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                               juce::MathConstants<float>::pi * 2.8f, true);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setTextValueSuffix("%");
    slider.setMouseDragSensitivity(macro ? 240 : 180);
    slider.setWantsKeyboardFocus(true);
    if (auto* parameter = state.getParameter(id))
        slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
    addAndMakeVisible(slider);
}
void GhostKnob::resized() { slider.setBounds(getLocalBounds()); }
