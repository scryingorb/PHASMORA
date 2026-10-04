#include "PluginProcessor.h"
#include "UI/GhostVisualizer.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

void testVisualizer();
void testPresets();

namespace
{
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
juce::Component* find(juce::Component& parent, const juce::String& id)
{
    if (parent.getComponentID() == id) return &parent;
    for (auto* child : parent.getChildren())
        if (auto* result = find(*child, id)) return result;
    return nullptr;
}
void phase(GhostAudioProcessor& processor, juce::AudioProcessorEditor& editor, int frames)
{
    auto* visualizer = dynamic_cast<GhostVisualizer*>(find(editor, "ghostVisualizer"));
    require(visualizer != nullptr, "visualizer is behind the controls");
    visualizer->setAnimationEnabled(false);
    juce::AudioBuffer<float> audio(2, 1600);
    juce::MidiBuffer midi;
    for (int frame = 0; frame < frames; ++frame)
    {
        for (int i = 0; i < 1600; ++i)
        {
            const auto sample = static_cast<float>(0.5 * std::sin((frame*1600+i)*0.017));
            audio.setSample(0, i, sample); audio.setSample(1, i, -sample*0.7f);
        }
        processor.processBlock(audio, midi);
        visualizer->advanceAnimation(1.0f/30.0f);
    }
}
void snapshot(juce::AudioProcessorEditor& editor, const juce::File& file, float scale = 1.0f)
{
    const auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true, scale);
    require(image.isValid(), "editor snapshot valid");
    if (file.existsAsFile()) return; // Preserve previous-stage preview artifacts.
    auto stream = file.createOutputStream();
    require(stream != nullptr, "preview output stream");
    stream->setPosition(0); stream->truncate();
    require(juce::PNGImageFormat().writeImageToStream(image, *stream), "preview PNG encoded");
}
}
int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        testVisualizer();
        testPresets();
        GhostAudioProcessor processor;
        processor.prepareToPlay(48000, 1600);
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        require(editor->getWidth() == 820 && editor->getHeight() == 540, "fixed editor dimensions");
        require(!editor->isResizable(), "editor intentionally fixed size");
        const auto directory = juce::File::getCurrentWorkingDirectory().getChildFile("UI-Previews-PHASMORA");
        require(directory.createDirectory().wasOk(), "preview directory created");
        phase(processor, *editor, 90);
        snapshot(*editor, directory.getChildFile("PHASMORA-default.png"));
        const std::array<const char*, 6> sliders { "ghost", "memory", "decay", "blur", "drift", "mix" };
        for (const auto* id : sliders)
        {
            auto* control = dynamic_cast<juce::Slider*>(find(*editor, id));
            require(control != nullptr && control->isVisible(), "all rotary controls exist");
            require(control->getTooltip().isNotEmpty(), "knob tooltip present");
            require(control->getMinimum() == 0 && control->getMaximum() == 100, "percentage ranges unchanged");
            auto* parameter = processor.apvts.getParameter(id);
            const auto initial = processor.apvts.getRawParameterValue(id)->load();
            require(std::abs(control->getValue() - initial) < 1.0e-5, "initial APVTS value reflected");
            control->setValue(73, juce::sendNotificationSync);
            require(std::abs(processor.apvts.getRawParameterValue(id)->load() - 73) < 1.0e-4f,
                    "UI gesture propagates to parameter");
            parameter->setValueNotifyingHost(0.21f);
            require(std::abs(control->getValue() - 21) < 1.0e-4, "host automation propagates to knob");
            require(control->getTextFromValue(21).contains("%"), "display is a percentage");
            require(control->keyPressed(juce::KeyPress(juce::KeyPress::upKey)), "knob supports keyboard adjustment");
            require(control->getValue() > 21, "arrow key raises knob value");
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
        }
        for (const auto* id : { "reverse", "freeze" })
        {
            auto* button = dynamic_cast<juce::ToggleButton*>(find(*editor, id));
            require(button != nullptr && button->getTooltip().isNotEmpty(), "toggle and tooltip exist");
            button->setToggleState(true, juce::sendNotificationSync);
            require(processor.apvts.getRawParameterValue(id)->load() == 1, "toggle writes existing bool");
            processor.apvts.getParameter(id)->setValueNotifyingHost(0);
            require(!button->getToggleState(), "host updates toggle state");
        }
        processor.apvts.getParameter("ghost")->setValueNotifyingHost(1);
        processor.apvts.getParameter("reverse")->setValueNotifyingHost(0);
        processor.apvts.getParameter("freeze")->setValueNotifyingHost(0);
        processor.apvts.getParameter("mix")->setValueNotifyingHost(1);
        phase(processor, *editor, 120);
        snapshot(*editor, directory.getChildFile("PHASMORA-active.png"));
        snapshot(*editor, directory.getChildFile("PHASMORA-active-2x.png"), 2);
        processor.apvts.getParameter("reverse")->setValueNotifyingHost(1);
        phase(processor, *editor, 90);
        snapshot(*editor, directory.getChildFile("PHASMORA-reverse.png"));
        processor.apvts.getParameter("freeze")->setValueNotifyingHost(1);
        phase(processor, *editor, 90);
        snapshot(*editor, directory.getChildFile("PHASMORA-freeze.png"));
        processor.apvts.getParameter("ghost")->setValueNotifyingHost(0);
        phase(processor, *editor, 120);
        snapshot(*editor, directory.getChildFile("PHASMORA-zero.png"));
        // Reopen an editor against restored host state: no ownership/attachment
        // leaks and no UI defaults overwriting saved parameter values.
        juce::MemoryBlock state;
        processor.getStateInformation(state);
        editor.reset();
        GhostAudioProcessor restored;
        restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        std::unique_ptr<juce::AudioProcessorEditor> reopened(restored.createEditor());
        auto* reverse = dynamic_cast<juce::ToggleButton*>(find(*reopened, "reverse"));
        auto* macro = dynamic_cast<juce::Slider*>(find(*reopened, "ghost"));
        require(reverse && reverse->getToggleState() && macro && macro->getValue() == 0,
                "reopened custom editor reflects restored state");
        require(restored.getParameters().size() == 8, "UI does not create parameters");
        std::cout << "GHOST UI: all eight attachments, host updates, percentages, tooltips, fixed bounds, reopen and 1x/2x rendering passed.\n";
        std::cout << "Previews: " << directory.getFullPathName() << '\n';
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
