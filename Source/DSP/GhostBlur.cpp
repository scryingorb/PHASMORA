#include "GhostBlur.h"
#include <algorithm>
#include <cmath>

void GhostBlur::AllPass::prepare(double sampleRate, double milliseconds)
{
    const auto length = static_cast<size_t>(std::max(1.0, std::round(sampleRate * milliseconds * 0.001)));
    delay.assign(length, 0.0f); // Only prepare allocates storage.
    position = 0;
}

void GhostBlur::AllPass::reset() noexcept
{
    std::fill(delay.begin(), delay.end(), 0.0f);
    position = 0;
}

float GhostBlur::AllPass::process(float input) noexcept
{
    if (delay.empty())
        return input;
    // Schroeder all-pass: H(z) = (z^-N - g) / (1 - g*z^-N).
    // Fixed |g| < 1: stable, unity magnitude, time/phase spreading. Do not
    // automate g or N; BLUR blends the complete, continuously running network.
    constexpr float g = 0.5f;
    const auto output = delay[position] - g * input;
    delay[position] = input + g * output;
    if (++position == delay.size())
        position = 0;
    return output;
}

void GhostBlur::prepare(double sampleRate)
{
    rate = sampleRate;
    // Unequal integer-rounded times decorrelate stereo without crossfeeding.
    // Three stages per channel, about 37-39 ms of total delay-line storage.
    constexpr double times[2][3] { { 3.7, 11.3, 23.9 }, { 4.3, 13.7, 19.1 } };
    for (size_t channel = 0; channel < diffusers.size(); ++channel)
        for (size_t stage = 0; stage < diffusers[channel].size(); ++stage)
            diffusers[channel][stage].prepare(rate, times[channel][stage]);
    pole.reset(rate, 0.15);
    filterBlend.reset(rate, 0.15);
    diffusionBlend.reset(rate, 0.15);
    updateTargets();
    reset();
}

void GhostBlur::reset() noexcept
{
    for (auto& state : filterState)
        state.fill(0.0f);
    for (auto& channel : diffusers)
        for (auto& stage : channel)
            stage.reset();
    pole.setCurrentAndTargetValue(pole.getTargetValue());
    filterBlend.setCurrentAndTargetValue(filterBlend.getTargetValue());
    diffusionBlend.setCurrentAndTargetValue(diffusionBlend.getTargetValue());
    framePole = static_cast<float>(pole.getCurrentValue());
    frameFilterBlend = static_cast<float>(filterBlend.getCurrentValue());
    frameDiffusionBlend = static_cast<float>(diffusionBlend.getCurrentValue());
}

void GhostBlur::setAmount(float normalizedBlur) noexcept
{
    if (!std::isfinite(normalizedBlur))
        return;
    const auto bounded = juce::jlimit(0.0f, 1.0f, normalizedBlur);
    if (bounded == amount)
        return;
    amount = bounded;
    updateTargets();
}

void GhostBlur::updateTargets() noexcept
{
    if (rate <= 0.0)
        return;
    const auto b = static_cast<double>(amount);
    const auto cutoff = std::min(20000.0 * std::pow(0.1, b), rate * 0.45);
    // Exact digital -3 dB cutoff for y=(1-p)*x+p*yPrevious. Solving
    // |H(exp(j*w))|^2 = 1/2 gives p = 1+a-sqrt(a*(a+2)), a=1-cos(w).
    const auto a = 1.0 - std::cos(juce::MathConstants<double>::twoPi * cutoff / rate);
    pole.setTargetValue(1.0 + a - std::sqrt(a * (a + 2.0)));
    // Ease out the transparent bypass over the first quarter of the knob.
    // Above 25% the stated cutoff is the actual voice filter cutoff.
    filterBlend.setTargetValue(std::min(1.0, 4.0 * b));
    diffusionBlend.setTargetValue(b * b);
}

void GhostBlur::beginFrame() noexcept
{
    framePole = static_cast<float>(pole.getNextValue());
    frameFilterBlend = static_cast<float>(filterBlend.getNextValue());
    frameDiffusionBlend = static_cast<float>(diffusionBlend.getNextValue());
}

void GhostBlur::filterVoice(size_t voice, float& left, float& right) noexcept
{
    auto& state = filterState[voice];
    state[0] = (1.0f - framePole) * left + framePole * state[0];
    state[1] = (1.0f - framePole) * right + framePole * state[1];
    // State stays warm at zero, but an exact bypass preserves the previous DSP.
    if (frameFilterBlend > 0.0f)
    {
        left += frameFilterBlend * (state[0] - left);
        right += frameFilterBlend * (state[1] - right);
    }
}

void GhostBlur::diffuse(float& left, float& right) noexcept
{
    auto diffusedLeft = left;
    auto diffusedRight = right;
    for (size_t stage = 0; stage < diffusers[0].size(); ++stage)
    {
        diffusedLeft = diffusers[0][stage].process(diffusedLeft);
        diffusedRight = diffusers[1][stage].process(diffusedRight);
    }
    // Complementary weights avoid a +3 dB boost for correlated signals.
    // Network always runs, so entering/leaving bypass doesn't switch stale state.
    if (frameDiffusionBlend > 0.0f)
    {
        left += frameDiffusionBlend * (diffusedLeft - left);
        right += frameDiffusionBlend * (diffusedRight - right);
    }
}
