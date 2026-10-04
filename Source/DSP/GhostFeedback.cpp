#include "GhostFeedback.h"
#include <cmath>

void GhostFeedback::prepare(double sampleRate, double leftVoiceGainSum, double rightVoiceGainSum) noexcept
{
    inverseGainSum = { 1.0 / leftVoiceGainSum, 1.0 / rightVoiceGainSum };
    coefficient.reset(sampleRate, 0.1); // 100 ms, once per stereo frame.
    reset();
}

void GhostFeedback::reset() noexcept
{
    previousWet.fill(0.0f);
    coefficient.setCurrentAndTargetValue(coefficient.getTargetValue());
}

void GhostFeedback::setDecay(float normalizedDecay) noexcept
{
    if (!std::isfinite(normalizedDecay))
        return;
    const auto d = static_cast<double>(juce::jlimit(0.0f, 1.0f, normalizedDecay));
    coefficient.setTargetValue(maximumCoefficient * d * (2.0 - d));
}

void GhostFeedback::getReturn(float& left, float& right) noexcept
{
    const auto gain = coefficient.getNextValue();
    // Normalize the four voice weights rather than running four full-gain loops.
    // At BLUR zero these form a peak-contractive weighted average. BLUR's fixed
    // all-passes preserve spectral magnitude, not instantaneous peak bounds;
    // its low-pass and complementary diffusion blend add no steady-state gain.
    // The 0.95 coefficient leaves loop headroom; tests cover modulation/automation
    // and transient overshoot as well. tanh's slope is never greater than one.
    // Its unity small-signal slope preserves quiet tails, while asymptotes +/-1
    // prevent sustained input from building a very large stored level. This is
    // only on the return: dry input and the initial four voices are untouched.
    left = static_cast<float>(gain * std::tanh(previousWet[0] * inverseGainSum[0]));
    right = static_cast<float>(gain * std::tanh(previousWet[1] * inverseGainSum[1]));
}

void GhostFeedback::capture(float wetLeft, float wetRight) noexcept
{
    // Defence against corrupted upstream DSP state; ordinary audio never hits
    // this guard. No cross-channel routing and no final dry/wet output feedback.
    previousWet[0] = std::isfinite(wetLeft) ? wetLeft : 0.0f;
    previousWet[1] = std::isfinite(wetRight) ? wetRight : 0.0f;
}
