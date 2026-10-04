#include "PluginProcessor.h"
#include "Presets/GhostPresetManager.h"
#include "UI/GhostVisualizer.h"
#include <set>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
juce::Component* find(juce::Component& c, const juce::String& id)
{
    if (c.getComponentID() == id) return &c;
    for (auto* child : c.getChildren()) if (auto* result = find(*child, id)) return result;
    return nullptr;
}
void manuallySet(GhostAudioProcessor& p, const GhostPresetManager::Preset& preset)
{
    for (size_t i = 0; i < preset.values.size(); ++i)
    {
        auto* parameter = p.apvts.getParameter(GhostPresetManager::ids[i]);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(preset.values[i]));
    }
}
}
void testPresets()
{
    using Manager = GhostPresetManager;
    GhostAudioProcessor p;
    check(Manager::presets().size() == 10, "ten factory presets");
    check(Manager::matchingPreset(p.apvts) == -1, "startup is unchanged Custom");
    const std::array<float, 8> defaults {65,40,40,20,10,35,0,0};
    for (size_t i = 0; i < defaults.size(); ++i)
        check(p.apvts.getRawParameterValue(Manager::ids[i])->load() == defaults[i], "defaults preserved");
    check(!Manager::load(p.apvts, -1) && !Manager::load(p.apvts, 10), "invalid index rejected");
    const std::array<std::array<float,8>,10> expected {{
        {35,25,20,15,12,22,0,0}, {70,45,48,35,22,38,0,0},
        {75,80,60,55,28,45,0,0}, {85,55,72,70,40,50,0,0},
        {80,90,85,82,35,55,0,0}, {78,65,68,72,30,45,0,0},
        {82,60,50,42,25,45,1,0}, {58,38,38,24,16,32,0,0},
        {72,68,78,88,32,52,0,0}, {100,82,88,68,72,62,1,0}
    }};
    std::set<juce::String> names;
    for (int index = 0; index < 10; ++index)
    {
        const auto& preset = Manager::presets()[static_cast<size_t>(index)];
        check(preset.values == expected[static_cast<size_t>(index)], "documented factory values");
        check(names.insert(preset.name).second, "unique preset names");
        p.apvts.getParameter("freeze")->setValueNotifyingHost(1);
        check(Manager::load(p.apvts, index), "preset loaded");
        for (size_t i = 0; i < preset.values.size(); ++i)
        {
            const auto value = preset.values[i];
            check(value >= 0 && value <= (i < 6 ? 100 : 1), "preset range");
            check(std::abs(p.apvts.getRawParameterValue(Manager::ids[i])->load()-value) < 0.0001f, "expected preset parameter");
        }
        check(preset.values[7] == 0, "every factory preset releases freeze");
        check(preset.values[6] == (index == 6 || index == 9 ? 1 : 0), "reverse presets");
        check(Manager::matchingPreset(p.apvts) == index, "derived factory name");
        juce::MemoryBlock saved; p.getStateInformation(saved);
        GhostAudioProcessor loaded; loaded.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        check(Manager::matchingPreset(loaded.apvts) == index, "preset state roundtrip");
        loaded.apvts.getParameter("blur")->setValueNotifyingHost(0.1234f);
        loaded.getStateInformation(saved);
        GhostAudioProcessor edited; edited.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        check(std::abs(edited.apvts.getRawParameterValue("blur")->load()-12.34f) < 0.0001f, "edited sound restored");
        check(Manager::matchingPreset(edited.apvts) == -1, "edited state Custom");
    }
    for (const double rate : {44100.0, 48000.0, 88200.0, 96000.0})
    {
        GhostAudioProcessor selected, manual;
        selected.prepareToPlay(rate, 256); manual.prepareToPlay(rate, 256);
        juce::AudioBuffer<float> a(2,256), b(2,256); juce::MidiBuffer midi;
        float peak = 0;
        const int blocks = static_cast<int>(rate * 18 / 256);
        for (int block = 0; block < blocks; ++block)
        {
            // Populate history for 11 seconds, then rapidly cycle extreme settings.
            if (block == 0 || (block > rate*11/256 && block % 13 == 0))
            {
                const int index = block == 0 ? 9 : (block/13)%10;
                Manager::load(selected.apvts, index);
                manuallySet(manual, Manager::presets()[static_cast<size_t>(index)]);
            }
            for (int i = 0; i < 256; ++i)
            {
                const auto value = static_cast<float>(0.45 * std::sin((block*256+i)*0.019));
                a.setSample(0,i,value); a.setSample(1,i,-0.8f*value);
            }
            b.makeCopyOf(a); selected.processBlock(a,midi); manual.processBlock(b,midi);
            for (int channel = 0; channel < 2; ++channel)
            {
                check(std::memcmp(a.getReadPointer(channel),b.getReadPointer(channel),256*sizeof(float)) == 0,
                      "preset load bit-identical to manually set parameters; history retained");
                for (int i = 0; i < 256; ++i)
                { const auto v = a.getSample(channel,i); check(std::isfinite(v), "finite switching output"); peak = juce::jmax(peak,std::abs(v)); }
            }
        }
        check(peak < 1.0f, "switching stress no huge gain spike");
        std::cout << rate << " Hz preset switching peak=" << peak << " (" << juce::Decibels::gainToDecibels(peak) << " dBFS), bit-identical to manual parameters.\n";
    }
    Manager::load(p.apvts,5); p.prepareToPlay(48000,1600);
    juce::AudioBuffer<float> audio(2,1600); audio.clear(); juce::MidiBuffer midi;
    p.processBlock(audio,midi);
    check(audio.getMagnitude(0,1600) == 0, "Frozen Choir contains no captured sound");
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        auto* selector = dynamic_cast<juce::ComboBox*>(find(*editor,"factoryPreset"));
        auto* next = dynamic_cast<juce::TextButton*>(find(*editor,"nextPreset"));
        auto* previous = dynamic_cast<juce::TextButton*>(find(*editor,"previousPreset"));
        auto* visual = dynamic_cast<GhostVisualizer*>(find(*editor,"ghostVisualizer"));
        check(selector && next && previous && visual,"preset controls and visualizer present");
        selector->setSelectedId(2,juce::sendNotificationSync);
        check(Manager::matchingPreset(p.apvts) == 1,"selector applies Haunted Vocal");
        auto* knob = dynamic_cast<juce::Slider*>(find(*editor,"blur"));
        check(knob && std::abs(knob->getValue()-35)<0.0001,"preset updates attachment");
        next->onClick(); check(Manager::matchingPreset(p.apvts)==2,"next preset");
        previous->onClick(); check(Manager::matchingPreset(p.apvts)==1,"previous preset");
        selector->setSelectedId(10,juce::sendNotificationSync);
        next->onClick(); check(Manager::matchingPreset(p.apvts)==0,"next wraps");
        previous->onClick(); check(Manager::matchingPreset(p.apvts)==9,"previous wraps");
        knob->setValue(12,juce::sendNotificationSync);
        for (int attempt=0; attempt<100 && selector->getSelectedId()!=0; ++attempt)
        { juce::Thread::sleep(20); juce::Timer::callPendingTimersSynchronously(); }
        check(selector->getSelectedId()==0,"manual edit label Custom");
        next->onClick(); check(Manager::matchingPreset(p.apvts)==0,"Custom next starts Whisper");
        selector->setSelectedId(2,juce::sendNotificationSync);
        visual->setAnimationEnabled(false);
        for (int frame = 0; frame < 90; ++frame)
        {
            for (int i = 0; i < 1600; ++i)
            { const auto v=static_cast<float>(0.4*std::sin((frame*1600+i)*0.017)); audio.setSample(0,i,v); audio.setSample(1,i,-v); }
            p.processBlock(audio,midi); visual->advanceAnimation(1.0f/30);
        }
        if (cycle == 0)
        {
            const auto directory=juce::File::getCurrentWorkingDirectory().getChildFile("UI-Previews-PHASMORA");
            check(directory.createDirectory().wasOk(),"preview directory");
            auto stream=directory.getChildFile("PHASMORA-presets.png").createOutputStream();
            check(stream != nullptr,"preset preview stream"); stream->setPosition(0); stream->truncate();
            check(juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),*stream),"preset preview PNG");
        }
    }
    check(p.getTailLengthSeconds()>0.0 && p.getParameters().size()==8,"tail metadata positive and parameter count unchanged");
    std::cout << "All factory preset, state, switching, attachment, selector and editor lifecycle tests passed.\n";
}
