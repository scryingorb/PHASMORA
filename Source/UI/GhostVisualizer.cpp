#include "GhostVisualizer.h"
#include "GhostLookAndFeel.h"
#include "../PluginProcessor.h"
#include <algorithm>
#include <cmath>

namespace
{
float bounded(float value) noexcept { return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f; }
float approach(float current, float target, float dt, float seconds) noexcept
{ return current + (target-current) * (1.0f-std::exp(-dt/seconds)); }
}
GhostVisualizer::GhostVisualizer(GhostAudioProcessor& owner) : processor(owner)
{
    constexpr const char* ids[] { "ghost", "memory", "decay", "blur", "drift", "reverse", "freeze" };
    for (size_t i = 0; i < parameters.size(); ++i) parameters[i] = processor.apvts.getRawParameterValue(ids[i]);
    jassert(hasValidParameterBindings());
    for (auto& wisp : wisps)
        for (auto& path : wisp.trails) path.preallocateSpace(64);
    visibleArea.preallocateSpace(128);
    setComponentID("ghostVisualizer");
    setInterceptsMouseClicks(false, false);
    setAccessible(false); // Decorative, never a competing parameter/control.
    lastSequence = processor.getVisualMeterSequence();
    setAnimationEnabled(true);
}
GhostVisualizer::~GhostVisualizer() { stopTimer(); }
bool GhostVisualizer::hasValidParameterBindings() const noexcept
{
    return std::all_of(parameters.begin(), parameters.end(), [](const auto* p) { return p != nullptr; });
}
void GhostVisualizer::setAnimationEnabled(bool enabled)
{
    if (enabled)
    {
        previousTick = juce::Time::getMillisecondCounterHiRes();
        startTimerHz(framesPerSecond);
    }
    else stopTimer();
}
void GhostVisualizer::timerCallback()
{
    ++callbackCount;
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto dt = static_cast<float>((now-previousTick) * 0.001);
    previousTick = now;
    // Hidden editors retain a cheap timer but skip geometry/repaint work.
    if (isShowing()) advanceAnimation(dt);
}
void GhostVisualizer::advanceAnimation(float elapsedSeconds)
{
    if (!hasValidParameterBindings() || !std::isfinite(elapsedSeconds)) return;
    const auto dt = std::clamp(elapsedSeconds, 0.0f, 0.1f);
    const auto percent = [&](size_t index) { return bounded(parameters[index]->load(std::memory_order_relaxed) * 0.01f); };
    state.ghost = approach(state.ghost, percent(0), dt, 0.18f);
    state.memory = approach(state.memory, percent(1), dt, 0.4f);
    state.decay = approach(state.decay, percent(2), dt, 0.3f);
    state.blur = approach(state.blur, percent(3), dt, 0.3f);
    state.drift = approach(state.drift, percent(4), dt, 0.4f);
    const auto reverse = bounded(parameters[5]->load(std::memory_order_relaxed)) >= 0.5f;
    state.direction = approach(state.direction, reverse ? -1.0f : 1.0f, dt, 0.4f);
    state.freeze = approach(state.freeze, bounded(parameters[6]->load(std::memory_order_relaxed)), dt, 0.45f);
    const auto sequence = processor.getVisualMeterSequence();
    staleSeconds = sequence == lastSequence ? std::min(10.0f, staleSeconds + dt) : 0.0f;
    lastSequence = sequence;
    // Heartbeat prevents stale held meters glowing forever if a host suspends
    // audio callbacks. UI release still decays organically rather than flickering.
    const auto level = staleSeconds < 0.15f
        ? std::sqrt(std::max(bounded(processor.getVisualInputLevel()), 0.85f * bounded(processor.getVisualOutputLevel()))) : 0.0f;
    const auto release = 0.25f + 1.75f * state.decay + 0.8f * state.freeze;
    state.energy = approach(state.energy, level, dt, level > state.energy ? 0.12f : release);
    state.intensity = 0.003f + std::pow(state.ghost, 1.6f) * (0.035f + 0.28f * state.energy);
    state.speed = (0.38f + 0.16f*state.drift) * state.direction * (1.0f - 0.82f*state.freeze);
    state.phase += static_cast<double>(dt * state.speed);
    // Keep long sessions numerically small. All procedural factors below use
    // integer harmonics, so this wrap does not introduce a geometry jump.
    state.phase = std::remainder(state.phase, juce::MathConstants<double>::twoPi);
    updateGeometry();
    repaint();
}
void GhostVisualizer::resized()
{
    visibleArea.clear();
    visibleArea.setUsingNonZeroWinding(false);
    visibleArea.addRectangle(getLocalBounds().toFloat());
    // Keep the dial face, percentage, pointer and inner ring completely clear.
    visibleArea.addEllipse(getWidth()*0.5f-97, getHeight()*0.5f+2-97, 194, 194);
    updateGeometry();
}
void GhostVisualizer::updateGeometry()
{
    const auto cx = getWidth()*0.5f;
    const auto cy = getHeight()*0.5f+2;
    const auto phase = static_cast<float>(state.phase);
    for (size_t i = 0; i < wisps.size(); ++i)
    {
        auto& wisp = wisps[i];
        const auto seed = static_cast<float>(i) * 2.39996323f;
        wisp.opacity = 0.6f + 0.4f * (0.5f+0.5f*std::sin(phase+seed));
        for (size_t echo = 0; echo < wisp.trails.size(); ++echo)
        {
            auto& path = wisp.trails[echo];
            path.clear();
            const auto t = phase - state.speed * static_cast<float>(echo) * (0.16f+0.4f*state.decay);
            const auto start = seed + t;
            const auto extent = 0.7f + 0.5f*(0.5f+0.5f*std::sin(t+seed)) + state.energy*0.18f;
            std::array<juce::Point<float>, 7> points;
            for (size_t j = 0; j < points.size(); ++j)
            {
                const auto u = static_cast<float>(j)/6.0f;
                const auto angle = start + u * extent;
                const auto wander = state.drift * (7*std::sin(angle*3+t) + 4*std::cos(angle*2-t));
                const auto radius = 111 + 14*state.memory + (static_cast<float>(i%3)-1)*7
                    + state.energy*6*std::sin(angle*2+t) + wander;
                points[j] = { cx + std::cos(angle)*radius + state.drift*4*std::sin(t+seed),
                              cy + std::sin(angle)*radius*0.78f + 3*std::sin(angle*2-t) };
            }
            path.startNewSubPath(points[0]);
            path.cubicTo(points[1], points[2], points[3]);
            path.cubicTo(points[4], points[5], points[6]);
        }
    }
    for (size_t i = 0; i < particles.size(); ++i)
    {
        const auto seed = static_cast<float>(i)*2.39996323f;
        const auto angle = phase + seed;
        const auto radius = 123+12*state.memory+7*std::sin(phase*2+seed);
        particles[i].centre = { cx+radius*std::cos(angle), cy+0.78f*radius*std::sin(angle) };
        particles[i].radius = 0.7f+0.9f*state.energy;
        particles[i].opacity = 0.25f+0.5f*(0.5f+0.5f*std::sin(phase*2+seed));
    }
}
void GhostVisualizer::paint(juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState save(g);
    g.reduceClipRegion(visibleArea);
    const auto tint = GhostTheme::mint.interpolatedWith(GhostTheme::freeze, state.freeze*0.75f);
    const auto softness = 1.0f+state.blur*4.5f;
    for (const auto& wisp : wisps)
        for (int echo = 2; echo >= 0; --echo)
        {
            const auto echoGain = echo == 0 ? 1.0f : (0.10f+state.decay*0.28f)/static_cast<float>(echo);
            const auto alpha = state.intensity*wisp.opacity*echoGain;
            const auto& path = wisp.trails[static_cast<size_t>(echo)];
            g.setColour(tint.withAlpha(alpha*0.12f));
            g.strokePath(path, juce::PathStrokeType(softness*4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(tint.withAlpha(alpha*0.30f));
            g.strokePath(path, juce::PathStrokeType(softness*1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(tint.withAlpha(alpha*(0.90f-0.55f*state.blur)));
            g.strokePath(path, juce::PathStrokeType(0.8f+0.8f*state.blur, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    for (const auto& particle : particles)
    {
        const auto x = particle.centre.x, y = particle.centre.y, r = particle.radius;
        const auto alpha = bounded(state.intensity*particle.opacity*2.0f);
        g.setColour(tint.withAlpha(alpha*0.08f));
        g.fillEllipse(x-r*3, y-r*3, r*6, r*6);
        g.setColour(GhostTheme::pale.withAlpha(alpha*(1.0f-state.freeze)));
        g.fillEllipse(x-r, y-r, r*2, r*2);
        // Freeze gradually replaces drifting points with held crystalline marks.
        g.setColour(GhostTheme::freeze.withAlpha(alpha*state.freeze));
        g.drawLine(x-r*2, y, x, y-r*2, 0.7f); g.drawLine(x, y-r*2, x+r*2, y, 0.7f);
        g.drawLine(x+r*2, y, x, y+r*2, 0.7f); g.drawLine(x, y+r*2, x-r*2, y, 0.7f);
    }
}
