#include "GhostVoice.h"
#include <algorithm>
#include <cmath>

void GhostVoice::prepare(double sampleRate, double maxDelaySeconds, double initialDelaySeconds,
                         float gainDecibels, float pan, double lfoRateHz,
                         double initialPhaseCycles, GhostReverseSettings reverseSettings) noexcept
{
    rate = sampleRate;
    maxDelaySamples = rate * maxDelaySeconds;
    fadeLength = std::max(1, static_cast<int>(rate * 0.1)); // 100 ms.
    lfoHz = std::isfinite(lfoRateHz) ? std::clamp(lfoRateHz, 0.03, 0.5) : 0.07;
    initialPhase = std::isfinite(initialPhaseCycles)
        ? initialPhaseCycles - std::floor(initialPhaseCycles) : 0.0;
    phaseStep = lfoHz / rate;
    depth.reset(rate, 0.15); // 150 ms, independent of MEMORY and DECAY smoothing.
    setDrift(driftAmount);
    const auto maximumDrift = rate * (1.0 - std::exp2(-50.0 / 1200.0))
                            / (juce::MathConstants<double>::twoPi * lfoHz);
    reverse.prepare(rate, maximumDrift, reverseSettings);
    reverseBlend.reset(rate, 0.1); // Independent 100 ms mode transition.
    setDelaySeconds(initialDelaySeconds);
    reset();

    basePan = static_cast<double>(std::clamp(pan, -1.0f, 1.0f));
    linearGain = std::pow(10.0, static_cast<double>(gainDecibels) / 20.0);
    stereoWidth = -1.0; // Force freshly prepared gain calculation.
    setStereoWidth(1.0);
}

void GhostVoice::setStereoWidth(double normalizedWidth) noexcept
{
    if (!std::isfinite(normalizedWidth)) return;
    const auto width = std::clamp(normalizedWidth, 0.0, 1.0);
    if (width == stereoWidth) return;
    stereoWidth = width;
    constexpr double pi = 3.14159265358979323846;
    const auto angle = (basePan * width + 1.0) * pi / 4.0;
    // Width changes only the voice's balance gains. Never sum/crossfeed channels.
    leftGain = static_cast<float>(linearGain * std::cos(angle));
    rightGain = static_cast<float>(linearGain * std::sin(angle));
}

void GhostVoice::reset() noexcept
{
    currentDelay = destinationDelay = requestedDelay;
    fadePosition = 0;
    fading = false;
    phase = initialPhase; // Only prepare/reset restarts deterministic modulation.
    depth.setCurrentAndTargetValue(depth.getTargetValue());
    readDelay = destinationReadDelay = currentDelay;
    reverse.reset();
    reverseBlend.setCurrentAndTargetValue(reverseBlend.getTargetValue());
}

void GhostVoice::setDrift(float normalizedDrift) noexcept
{
    if (!std::isfinite(normalizedDrift))
        return;
    driftAmount = std::clamp(normalizedDrift, 0.0f, 1.0f);
    const auto d = static_cast<double>(driftAmount);
    const auto cents = 50.0 * d * d;
    // readTime = time - delay(time), so playback speed = 1 - delay'(time).
    // A*sin(2*pi*f*t) has maximum slope A*2*pi*f. Choose q so the slowest
    // playback speed is exactly 2^(-cents/1200). The faster side is slightly
    // asymmetric in cents (+48.596 at the -50-cent endpoint).
    const auto q = 1.0 - std::exp2(-cents / 1200.0);
    depth.setTargetValue(rate * q / (juce::MathConstants<double>::twoPi * lfoHz));
}

double GhostVoice::modulatedDelay(double baseDelay, double amplitude, double sine) const noexcept
{
    // Bound amplitude, NOT the sine waveform: this avoids flat/clipped LFO tops.
    // Each crossfade head has its own static margin, but shares phase and depth.
    // Normal GHOST settings need no reduction: even voice 1's maximum 64.73 ms
    // excursion fits inside its shortest 100 ms base delay.
    const auto margin = std::max(0.0, std::min(baseDelay, maxDelaySamples - baseDelay));
    return baseDelay + std::min(amplitude, margin) * sine;
}

void GhostVoice::setDelaySeconds(double seconds) noexcept
{
    if (std::isfinite(seconds))
    {
        requestedDelay = std::clamp(seconds * rate, 0.0, maxDelaySamples);
        reverse.setDelaySamples(requestedDelay);
    }
}

void GhostVoice::processSample(const GhostBuffer& history, float& wetLeft, float& wetRight) noexcept
{
    // Freeze both BASE delays during a transition; DRIFT keeps moving around
    // those bases with one shared LFO. MEMORY automation only updates
    // the pending request; it cannot restart a fade or jump an audible head.
    // On completion the latest request starts the next fade. This coalesces fast
    // automation with bounded work and at most ~200 ms settling after a change.
    if (!fading && requestedDelay != currentDelay)
    {
        destinationDelay = requestedDelay;
        fadePosition = 0;
        fading = true;
    }

    const auto amplitude = depth.getNextValue();
    const auto sine = std::sin(juce::MathConstants<double>::twoPi * phase);
    // One phase update per voice per stereo frame, even at zero DRIFT. Wrapping
    // cycles keeps the argument small; sin(2*pi) == sin(0), so no wrap step.
    phase += phaseStep;
    if (phase >= 1.0)
        phase -= 1.0;
    readDelay = modulatedDelay(currentDelay, amplitude, sine);
    destinationReadDelay = modulatedDelay(destinationDelay, amplitude, sine);
    auto left = history.readDelayedSample(0, readDelay);
    auto right = history.readDelayedSample(1, readDelay);
    if (fading)
    {
        const auto fraction = static_cast<float>(++fadePosition) / static_cast<float>(fadeLength);
        // Complementary linear weights avoid the +3 dB boost an equal-power
        // crossfade would create when two history positions are correlated.
        left += fraction * (history.readDelayedSample(0, destinationReadDelay) - left);
        right += fraction * (history.readDelayedSample(1, destinationReadDelay) - right);
        if (fadePosition == fadeLength)
        {
            currentDelay = destinationDelay;
            fading = false;
        }
    }

    float reverseLeft = 0.0f, reverseRight = 0.0f;
    // Keep region scheduling warm while OFF, so toggles don't reset other DSP.
    // Reverse source speed is -1 - offset'(t); forward speed is 1 - offset'(t).
    // Same phase/depth and same bounded cents range, with reversed pitch polarity.
    reverse.processSample(history, amplitude * sine, reverseLeft, reverseRight);
    const auto mode = static_cast<float>(reverseBlend.getNextValue());
    if (mode > 0.0f)
    {
        left += mode * (reverseLeft - left);
        right += mode * (reverseRight - right);
    }
    wetLeft += left * leftGain;
    wetRight += right * rightGain;
}
