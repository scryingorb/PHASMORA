#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <limits>

static_assert(std::atomic<float>::is_always_lock_free, "Visual meters require lock-free float atomics");
static_assert(std::atomic<unsigned int>::is_always_lock_free, "Visual heartbeat requires lock-free atomics");

GhostAudioProcessor::GhostAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "GHOST_STATE", createParameterLayout())
{
    ghostParameter = apvts.getRawParameterValue("ghost");
    mixParameter = apvts.getRawParameterValue("mix");
    memoryParameter = apvts.getRawParameterValue("memory");
    decayParameter = apvts.getRawParameterValue("decay");
    driftParameter = apvts.getRawParameterValue("drift");
    blurParameter = apvts.getRawParameterValue("blur");
    freezeParameter = apvts.getRawParameterValue("freeze");
    reverseParameter = apvts.getRawParameterValue("reverse");
}

juce::AudioProcessorValueTreeState::ParameterLayout GhostAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto addPercent = [&layout](const char* id, const char* name, float defaultValue)
    {
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> { 0.0f, 100.0f }, defaultValue,
            juce::AudioParameterFloatAttributes().withLabel("%")));
    };
    addPercent("ghost", "PHASMORA", 65.0f);
    addPercent("memory", "Memory", 40.0f);
    addPercent("decay", "Decay", 40.0f);
    addPercent("blur", "Blur", 20.0f);
    addPercent("drift", "Drift", 10.0f);
    addPercent("mix", "Mix", 35.0f);
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "reverse", 1 }, "Reverse", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "freeze", 1 }, "Freeze", false));
    return layout;
}

void GhostAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine.setGhost(ghostParameter->load(std::memory_order_relaxed) * 0.01f);
    engine.setMix(mixParameter->load(std::memory_order_relaxed) * 0.01f);
    engine.setMemory(memoryParameter->load(std::memory_order_relaxed) * 0.01f);
    engine.setDecay(decayParameter->load(std::memory_order_relaxed) * 0.01f);
    engine.setDrift(driftParameter->load(std::memory_order_relaxed) * 0.01f);
    engine.setBlur(blurParameter->load(std::memory_order_relaxed) * 0.01f);
    engine.setReverse(reverseParameter->load(std::memory_order_relaxed) >= 0.5f);
    engine.setFreeze(freezeParameter->load(std::memory_order_relaxed) >= 0.5f);
    engine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    meterSampleRate = sampleRate;
    clearVisualMeters();
}

void GhostAudioProcessor::releaseResources() { engine.reset(); clearVisualMeters(); }
void GhostAudioProcessor::reset() { engine.reset(); clearVisualMeters(); }

void GhostAudioProcessor::clearVisualMeters() noexcept
{
    inputEnvelope = outputEnvelope = 0.0f;
    inputLevel.store(0.0f, std::memory_order_relaxed);
    outputLevel.store(0.0f, std::memory_order_relaxed);
    meterSequence.fetch_add(1, std::memory_order_relaxed);
}

float GhostAudioProcessor::measurePeak(const juce::AudioBuffer<float>& buffer) noexcept
{
    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer(channel);
        for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
        {
            const auto value = samples[frame];
            if (std::isfinite(value)) peak = juce::jmax(peak, std::abs(value));
        }
    }
    return juce::jmin(1.0f, peak); // Display ceiling only; never writes audio.
}

bool GhostAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void GhostAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    // Cached atomic parameter pointer: no string lookup or locks on this thread.
    engine.setGhost(ghostParameter->load(std::memory_order_relaxed) * 0.01f);
    engine.setMix(mixParameter->load(std::memory_order_relaxed) * 0.01f);
    engine.setMemory(memoryParameter->load(std::memory_order_relaxed) * 0.01f);
    engine.setDecay(decayParameter->load(std::memory_order_relaxed) * 0.01f);
    engine.setDrift(driftParameter->load(std::memory_order_relaxed) * 0.01f);
    engine.setBlur(blurParameter->load(std::memory_order_relaxed) * 0.01f);
    engine.setReverse(reverseParameter->load(std::memory_order_relaxed) >= 0.5f);
    engine.setFreeze(freezeParameter->load(std::memory_order_relaxed) >= 0.5f);
    const auto inputPeak = measurePeak(buffer);
    engine.processBlock(buffer); // Existing audio order and samples are untouched.
    if (buffer.getNumSamples() == 0) return; // No new audio heartbeat for empty blocks.
    const auto outputPeak = measurePeak(buffer);
    const auto release = static_cast<float>(std::exp(-buffer.getNumSamples() / (meterSampleRate * 0.25)));
    inputEnvelope = juce::jmax(inputPeak, inputEnvelope * release);
    outputEnvelope = juce::jmax(outputPeak, outputEnvelope * release);
    if (inputEnvelope < 1.0e-8f) inputEnvelope = 0.0f;
    if (outputEnvelope < 1.0e-8f) outputEnvelope = 0.0f;
    inputLevel.store(inputEnvelope, std::memory_order_relaxed);
    outputLevel.store(outputEnvelope, std::memory_order_relaxed);
    meterSequence.fetch_add(1, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* GhostAudioProcessor::createEditor()
{
    return new GhostAudioProcessorEditor(*this);
}

void GhostAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void GhostAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GhostAudioProcessor();
}

// Metadata only. Never gate, reset or truncate the DSP based on this estimate.
// Five seconds per effective loop allows margin over the longest-channel
// weighted mean age (4.613 s at full MEMORY), including short diffusion.
// Always reserve 12 s for existing history/reader transitions even at DECAY=0.
// JUCE's VST3 wrapper maps +infinity to Steinberg::Vst::kInfiniteTail.
double GhostAudioProcessor::getTailLengthSeconds() const
{
    if (freezeParameter->load(std::memory_order_relaxed) >= 0.5f)
        return std::numeric_limits<double>::infinity();
    const auto normalized = [](float value, double fallback)
    { return std::isfinite(value) ? juce::jlimit(0.0, 1.0, value * 0.01) : fallback; };
    const auto g = normalized(ghostParameter->load(std::memory_order_relaxed), 1.0);
    const auto d = normalized(decayParameter->load(std::memory_order_relaxed), 1.0) * g * g;
    // 8% margin covers gain-sum differences as macro stereo width narrows.
    const auto q = juce::jmin(0.95, 1.08 * 0.95 * d * (2.0-d) * g * g);
    if (q <= 0.0) return 12.0;
    return juce::jlimit(12.0, maximumFiniteTailSeconds,
                       12.0 + std::ceil(5.0 * std::log(0.001) / std::log(q)));
}
