#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// The complete visual palette lives here. No theme colours belong in the editor.
struct GhostTheme
{
    inline static const juce::Colour black { 0xff000000 };
    inline static const juce::Colour background { 0xff040908 };
    inline static const juce::Colour panel { 0xff0b1714 };
    inline static const juce::Colour body { 0xff101e19 };
    inline static const juce::Colour deepGreen { 0xff123e30 };
    inline static const juce::Colour edge { 0xff275646 };
    inline static const juce::Colour mint { 0xff52ffc0 };
    inline static const juce::Colour pale { 0xffdbfff2 };
    inline static const juce::Colour freeze { 0xffa0ffe9 };
    inline static const juce::Colour text { 0xffbdd9cd };
    inline static const juce::Colour muted { 0xff74988a };
};

class GhostLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GhostLookAndFeel();
    static juce::Font font(float height, bool bold = false);
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawTooltip(juce::Graphics&, const juce::String&, int, int) override;
    void paintBackdrop(juce::Graphics&, juce::Rectangle<int>) const;
};
