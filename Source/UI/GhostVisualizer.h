#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>
class GhostAudioProcessor;

// Message-thread-only renderer. The audio processor owns the telemetry and can
// operate with no editor/visualizer at all. All particles/paths have fixed storage.
class GhostVisualizer : public juce::Component, private juce::Timer
{
public:
    explicit GhostVisualizer(GhostAudioProcessor&);
    ~GhostVisualizer() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    static constexpr int framesPerSecond = 30, wispCount = 12, particleCount = 12;
    struct VisualState
    {
        float ghost = 0, memory = 0, decay = 0, blur = 0, drift = 0;
        float freeze = 0, direction = 1, energy = 0, intensity = 0, speed = 0;
        double phase = 0;
    };
    VisualState getVisualState() const noexcept { return state; }
    bool hasValidParameterBindings() const noexcept;
    unsigned int getTimerCallbackCount() const noexcept { return callbackCount; }
    bool isAnimationRunning() const noexcept { return isTimerRunning(); }
    void setAnimationEnabled(bool enabled);
    // Deterministic same-frame update for previews/tests; never called by audio.
    void advanceAnimation(float elapsedSeconds);
private:
    void timerCallback() override;
    void updateGeometry();
    GhostAudioProcessor& processor;
    std::array<const std::atomic<float>*, 7> parameters {};
    VisualState state;
    struct Wisp { std::array<juce::Path, 3> trails; float opacity = 0; };
    struct Particle { juce::Point<float> centre; float radius = 0, opacity = 0; };
    std::array<Wisp, wispCount> wisps;
    std::array<Particle, particleCount> particles;
    juce::Path visibleArea;
    unsigned int lastSequence = 0, callbackCount = 0;
    float staleSeconds = 0;
    double previousTick = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GhostVisualizer)
};
