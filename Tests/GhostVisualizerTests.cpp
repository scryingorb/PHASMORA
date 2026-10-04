#include "PluginProcessor.h"
#include "UI/GhostVisualizer.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void set(GhostAudioProcessor& p, const char* id, float value) { p.apvts.getParameter(id)->setValueNotifyingHost(value); }
void sync(GhostAudioProcessor& p, GhostEngine& reference)
{
    const auto value = [&](const char* id) { return p.apvts.getRawParameterValue(id)->load() * 0.01f; };
    reference.setGhost(value("ghost")); reference.setMemory(value("memory")); reference.setMix(value("mix"));
    reference.setDecay(value("decay")); reference.setDrift(value("drift")); reference.setBlur(value("blur"));
    reference.setReverse(p.apvts.getRawParameterValue("reverse")->load() >= 0.5f);
    reference.setFreeze(p.apvts.getRawParameterValue("freeze")->load() >= 0.5f);
}
void meterRegression()
{
    for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
        for (const int size : { 32, 64, 256, 512, 1024 })
        {
            GhostAudioProcessor processor;
            for (const auto* id : { "ghost", "decay", "drift", "blur" }) set(processor, id, 1);
            set(processor, "memory", 0); set(processor, "mix", 0.35f); set(processor, "reverse", 1);
            GhostEngine reference;
            sync(processor, reference);
            processor.prepareToPlay(rate, size); reference.prepare(rate, size, 2);
            juce::AudioBuffer<float> metered(2, size), unmetered(2, size);
            juce::MidiBuffer midi;
            for (int frame = 0; frame < static_cast<int>(rate*2); frame += size)
            {
                if (frame >= rate*0.5) set(processor, "freeze", 1);
                set(processor, "ghost", frame >= rate && frame < rate*1.5 ? 0.0f : 1.0f);
                sync(processor, reference);
                for (int channel = 0; channel < 2; ++channel)
                    for (int i = 0; i < size; ++i)
                    {
                        const auto value = static_cast<float>((channel == 0 ? 0.4 : -0.3) * std::sin(0.003*(frame+i)));
                        metered.setSample(channel, i, value); unmetered.setSample(channel, i, value);
                    }
                processor.processBlock(metered, midi); reference.processBlock(unmetered);
                for (int channel = 0; channel < 2; ++channel)
                    require(std::memcmp(metered.getReadPointer(channel), unmetered.getReadPointer(channel),
                                        static_cast<size_t>(size)*sizeof(float)) == 0, "metering must preserve audio bit-for-bit");
                require(std::isfinite(processor.getVisualInputLevel()) && processor.getVisualInputLevel() >= 0
                        && processor.getVisualInputLevel() <= 1 && std::isfinite(processor.getVisualOutputLevel())
                        && processor.getVisualOutputLevel() >= 0 && processor.getVisualOutputLevel() <= 1,
                        "display meters finite and bounded");
            }
            const auto sequence = processor.getVisualMeterSequence();
            juce::AudioBuffer<float> empty(2, 0);
            processor.processBlock(empty, midi);
            require(processor.getVisualMeterSequence() == sequence, "empty block does not hold visual heartbeat alive");
            processor.reset(); reference.reset();
            require(processor.getVisualInputLevel() == 0 && processor.getVisualOutputLevel() == 0, "reset clears visual meters");
            for (const float invalid : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
            {
                metered.clear(); unmetered.clear();
                metered.setSample(0, 0, invalid); unmetered.setSample(0, 0, invalid);
                processor.processBlock(metered, midi); reference.processBlock(unmetered);
                require(std::isfinite(processor.getVisualInputLevel()) && std::isfinite(processor.getVisualOutputLevel()),
                        "faulty upstream values cannot poison meters");
                require(std::memcmp(metered.getReadPointer(0), unmetered.getReadPointer(0), static_cast<size_t>(size)*sizeof(float)) == 0,
                        "meter fault guard does not change existing audio fault handling");
            }
            processor.releaseResources();
            require(processor.getVisualInputLevel() == 0 && processor.getVisualOutputLevel() == 0, "release clears display telemetry");
            std::cout << rate << " Hz / " << size << " samples: metered processor audio bit-identical to unmetered engine.\n";
        }
}
void visualStateTest()
{
    GhostAudioProcessor processor;
    processor.prepareToPlay(48000, 1600);
    GhostVisualizer visualizer(processor);
    visualizer.setBounds(0, 0, 476, 236);
    require(visualizer.hasValidParameterBindings() && visualizer.isAnimationRunning(), "visualizer binds existing parameters and starts 30 FPS timer");
    for (int attempt = 0; attempt < 100 && visualizer.getTimerCallbackCount() == 0; ++attempt)
    {
        juce::Thread::sleep(20);
        juce::Timer::callPendingTimersSynchronously();
    }
    require(visualizer.getTimerCallbackCount() > 0, "JUCE delivers actual visual timer callbacks");
    visualizer.setAnimationEnabled(false);
    require(!visualizer.isAnimationRunning(), "visualizer timer stops explicitly");
    juce::AudioBuffer<float> audio(2, 1600);
    juce::MidiBuffer midi;
    const auto advance = [&](int frames, bool sound)
    {
        for (int frame = 0; frame < frames; ++frame)
        {
            if (sound)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < audio.getNumSamples(); ++i) audio.setSample(c, i, 0.4f);
                processor.processBlock(audio, midi);
            }
            visualizer.advanceAnimation(1.0f/30.0f);
        }
    };
    set(processor, "ghost", 0); advance(90, true);
    const auto zero = visualizer.getVisualState();
    require(zero.intensity < 0.004f, "GHOST zero keeps only faint ambient motion");
    set(processor, "ghost", 1); advance(90, true);
    const auto active = visualizer.getVisualState();
    require(active.intensity > zero.intensity*20 && active.energy > 0.5f && active.direction > 0.99f,
            "GHOST/audio produce clear energy and forward circulation");
    set(processor, "reverse", 1);
    visualizer.advanceAnimation(1.0f/30.0f);
    require(visualizer.getVisualState().direction > 0, "REVERSE must not snap immediately");
    advance(90, true);
    const auto reversed = visualizer.getVisualState();
    require(reversed.direction < -0.99f && reversed.speed < 0, "REVERSE smoothly changes travel direction");
    set(processor, "freeze", 1); advance(90, true);
    const auto held = visualizer.getVisualState();
    require(held.freeze > 0.99f && std::abs(held.speed) > 0 && std::abs(held.speed) < std::abs(reversed.speed)*0.25f,
            "FREEZE slows but never stops visual travel");
    visualizer.setAnimationEnabled(true);
    require(visualizer.isAnimationRunning(), "FREEZE leaves timer enabled");
    const auto beforeTick = visualizer.getTimerCallbackCount();
    for (int attempt = 0; attempt < 100 && visualizer.getTimerCallbackCount() == beforeTick; ++attempt)
    {
        juce::Thread::sleep(20);
        juce::Timer::callPendingTimersSynchronously();
    }
    require(visualizer.getTimerCallbackCount() > beforeTick, "FREEZE retains real timer delivery");
    visualizer.setAnimationEnabled(false);
    for (const auto* id : { "memory", "decay", "blur", "drift" }) set(processor, id, 1);
    advance(90, true);
    const auto all = visualizer.getVisualState();
    require(all.memory > 0.99f && all.decay > 0.99f && all.blur > 0.99f && all.drift > 0.99f,
            "visual controls follow existing parameters");
    advance(900, false); // Host stopped invoking audio; stale meter must fade.
    require(visualizer.getVisualState().energy < 0.001f, "stale audio heartbeat fades visual envelope");
    require(std::isfinite(visualizer.getVisualState().phase), "motion phase finite after long visual hold");
    for (int i = 0; i < 10; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        auto* v = dynamic_cast<GhostVisualizer*>(editor->findChildWithID("ghostVisualizer"));
        require(v && v->isAnimationRunning(), "reopened editor starts its own visual timer");
        juce::Component::SafePointer<GhostVisualizer> weak(v);
        editor.reset();
        juce::Timer::callPendingTimersSynchronously();
        require(weak == nullptr, "closed editor destroys visualizer and stops its timer");
        audio.clear(); processor.processBlock(audio, midi); // No editor required.
    }
    require(processor.getParameters().size() == 8 && processor.getTailLengthSeconds() > 0.0,
            "visualization adds no parameters or host-tail change");
    std::cout << "Visualizer state, direction, freeze, stale-meter decay, bindings and 10 open/close cycles passed.\n";
}
}
void testVisualizer() { meterRegression(); visualStateTest(); }
