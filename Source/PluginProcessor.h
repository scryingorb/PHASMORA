#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/GhostEngine.h"

class GhostAudioProcessor : public juce::AudioProcessor
{
public:
    GhostAudioProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;
    static constexpr double maximumFiniteTailSeconds = 686.0;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Read-only display telemetry. These are input and FINAL OUTPUT peaks,
    // not an isolated wet meter (the verified engine intentionally stays intact).
    float getVisualInputLevel() const noexcept { return inputLevel.load(std::memory_order_relaxed); }
    float getVisualOutputLevel() const noexcept { return outputLevel.load(std::memory_order_relaxed); }
    unsigned int getVisualMeterSequence() const noexcept { return meterSequence.load(std::memory_order_relaxed); }
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    GhostEngine engine;
    std::atomic<float> inputLevel { 0.0f }, outputLevel { 0.0f };
    std::atomic<unsigned int> meterSequence { 0 };
    double meterSampleRate = 44100.0;
    float inputEnvelope = 0.0f, outputEnvelope = 0.0f;
    static float measurePeak(const juce::AudioBuffer<float>&) noexcept;
    void clearVisualMeters() noexcept;
    std::atomic<float>* ghostParameter = nullptr;
    std::atomic<float>* mixParameter = nullptr;
    std::atomic<float>* memoryParameter = nullptr;
    std::atomic<float>* decayParameter = nullptr;
    std::atomic<float>* driftParameter = nullptr;
    std::atomic<float>* blurParameter = nullptr;
    std::atomic<float>* freezeParameter = nullptr;
    std::atomic<float>* reverseParameter = nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GhostAudioProcessor)
};
