#include "PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void setParameter(GhostAudioProcessor& processor, const char* id, float normalized)
{
    auto* parameter = processor.apvts.getParameter(id);
    require(parameter != nullptr, "existing parameter missing");
    parameter->setValueNotifyingHost(normalized);
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    try
    {
        GhostAudioProcessor saved;
        setParameter(saved, "reverse", 1.0f);
        setParameter(saved, "memory", 0.0f);
        setParameter(saved, "decay", 0.0f);
        setParameter(saved, "drift", 0.0f);
        setParameter(saved, "blur", 0.0f);
        setParameter(saved, "mix", 1.0f);
        juce::MemoryBlock state;
        saved.getStateInformation(state);

        GhostAudioProcessor restored;
        restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        require(restored.apvts.getRawParameterValue("reverse")->load() == 1.0f,
                "REVERSE must restore enabled before prepare");
        require(restored.getParameters().size() == 8, "no redundant REVERSE parameter");
        require(restored.getTailLengthSeconds() > 0.0, "tail metadata positive");

        for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
        {
            saved.prepareToPlay(rate, 128);
            restored.prepareToPlay(rate, 128);
            juce::AudioBuffer<float> a(2, 128), b(2, 128);
            juce::MidiBuffer midi;
            double outputEnergy = 0.0;
            for (int offset = 0; offset < static_cast<int>(rate * 3.0); offset += 128)
            {
                for (int i = 0; i < 128; ++i)
                {
                    const auto input = static_cast<float>(0.25 * std::sin(0.0007 * (offset + i)));
                    a.setSample(0, i, input);
                    a.setSample(1, i, -input);
                    b.setSample(0, i, input);
                    b.setSample(1, i, -input);
                }
                saved.processBlock(a, midi);
                restored.processBlock(b, midi);
                for (int channel = 0; channel < 2; ++channel)
                    for (int i = 0; i < 128; ++i)
                    {
                        require(std::isfinite(b.getSample(channel, i)), "restored reverse output finite");
                        require(std::abs(a.getSample(channel, i) - b.getSample(channel, i)) < 1.0e-6f,
                                "restored reverse must match explicitly enabled processor");
                        outputEnergy += static_cast<double>(b.getSample(channel, i)) * b.getSample(channel, i);
                    }
            }
            require(outputEnergy > 0.01, "restored reverse creates audio");
            // Host automation through the existing APVTS, then state round-trip.
            setParameter(restored, "reverse", 0.0f);
            for (int frame = 0; frame < static_cast<int>(rate * 0.15); frame += 128)
            {
                b.clear();
                restored.processBlock(b, midi);
            }
            restored.getStateInformation(state);
            GhostAudioProcessor off;
            off.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            require(off.apvts.getRawParameterValue("reverse")->load() == 0.0f,
                    "automated OFF state persists");
            setParameter(restored, "reverse", 1.0f);
            std::cout << rate << " Hz REVERSE APVTS enabled-load, automation and state round-trip passed.\n";
        }
        for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
        {
            GhostAudioProcessor original, loaded;
            setParameter(original, "freeze", 1);
            setParameter(original, "mix", 1);
            setParameter(original, "memory", 0);
            setParameter(original, "decay", 0);
            setParameter(original, "blur", 0);
            original.getStateInformation(state);
            require(state.getSize() < 8192, "parameter state must not contain captured audio");
            loaded.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            require(loaded.apvts.getRawParameterValue("freeze")->load() == 1, "FREEZE enabled state restores");
            loaded.prepareToPlay(rate, 64);
            juce::AudioBuffer<float> block(2, 64);
            juce::MidiBuffer midi;
            for (int phase = 0; phase < 4; ++phase)
            {
                if (phase == 1) setParameter(loaded, "freeze", 0);
                if (phase == 2) setParameter(loaded, "freeze", 1);
                if (phase == 3) loaded.reset();
                double energy = 0;
                for (int frame = 0; frame < static_cast<int>(rate * 2); frame += 64)
                {
                    for (int channel = 0; channel < 2; ++channel)
                        for (int i = 0; i < 64; ++i) block.setSample(channel, i, 0.25f);
                    loaded.processBlock(block, midi);
                    for (int i = 0; i < 64; ++i)
                    {
                        const auto sample = block.getSample(0, i);
                        require(std::isfinite(sample), "FREEZE state output finite");
                        energy += static_cast<double>(sample) * sample;
                    }
                }
                require(phase == 0 || phase == 3 ? energy == 0 : energy > 0.01,
                        "FREEZE load/reset silent; automated recapture audible");
            }
            loaded.getStateInformation(state);
            require(state.getSize() < 8192, "captured audio not serialized");
            original.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            require(original.apvts.getRawParameterValue("freeze")->load() == 1, "FREEZE saved ON");
            setParameter(loaded, "freeze", 0);
            loaded.getStateInformation(state);
            original.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            require(original.apvts.getRawParameterValue("freeze")->load() == 0, "FREEZE saved OFF");
            std::cout << rate << " Hz FREEZE APVTS load, automation, reset and round-trip passed.\n";
        }
        for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
        {
            GhostAudioProcessor processor;
            require(processor.apvts.getRawParameterValue("ghost")->load() == 65, "GHOST release default is 65%");
            setParameter(processor, "ghost", 0);
            setParameter(processor, "memory", 0);
            setParameter(processor, "mix", 1);
            setParameter(processor, "decay", 1);
            setParameter(processor, "drift", 1);
            setParameter(processor, "blur", 1);
            setParameter(processor, "reverse", 1);
            processor.prepareToPlay(rate, 64);
            juce::AudioBuffer<float> block(2, 64);
            juce::MidiBuffer midi;
            for (int phase = 0; phase < 3; ++phase)
            {
                if (phase == 1) setParameter(processor, "freeze", 1);
                if (phase == 2) setParameter(processor, "ghost", 1);
                double energy = 0;
                for (int frame = 0; frame < static_cast<int>(rate * 3); frame += 64)
                {
                    for (int i = 0; i < 64; ++i)
                    {
                        const auto source = static_cast<float>(0.3 * std::sin(0.001 * (frame + i)));
                        block.setSample(0, i, source); block.setSample(1, i, -0.5f * source);
                    }
                    processor.processBlock(block, midi);
                    for (int i = 0; i < 64; ++i)
                    { const auto value = block.getSample(0, i); energy += static_cast<double>(value) * value; }
                }
                require(phase < 2 ? energy == 0 : energy > 0.01, "GHOST parameter mutes wet and restores frozen reverse content");
                require(processor.apvts.getRawParameterValue("decay")->load() == 100
                        && processor.apvts.getRawParameterValue("blur")->load() == 100
                        && processor.apvts.getRawParameterValue("drift")->load() == 100
                        && processor.apvts.getRawParameterValue("mix")->load() == 100
                        && processor.apvts.getRawParameterValue("memory")->load() == 0
                        && processor.apvts.getRawParameterValue("reverse")->load() == 1,
                        "macro must never overwrite visible individual settings");
            }
            setParameter(processor, "ghost", 0.5f);
            processor.getStateInformation(state);
            GhostAudioProcessor restoredMacro;
            restoredMacro.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            require(restoredMacro.apvts.getRawParameterValue("ghost")->load() == 50, "macro state round-trip");
            require(restoredMacro.apvts.getRawParameterValue("freeze")->load() == 1, "macro preserves freeze state");
            require(restoredMacro.getParameters().size() == 8 && restoredMacro.getTailLengthSeconds() > 0.0,
                    "macro must preserve layout and tail declaration");
            std::cout << rate << " Hz GHOST APVTS default, automation, persistence and control independence passed.\n";
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
