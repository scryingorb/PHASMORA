#include "DSP/GhostBuffer.h"
#include "DSP/GhostEngine.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

void testFreeze();
void testMacro();

namespace
{
void near(float actual, float expected, const char* message, float tolerance = 1.0e-5f)
{
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance)
        throw std::runtime_error(std::string(message) + ": actual=" + std::to_string(actual)
                                 + ", expected=" + std::to_string(expected));
}

void testBuffer()
{
    GhostBuffer buffer;
    near(buffer.readDelayedSample(0, 0.0), 0.0f, "unprepared read");
    buffer.writeSample(0, 1.0f);
    buffer.prepare(4.0, 1.0, 2); // Five slots: now plus four previous frames.
    near(buffer.readDelayedSample(0, 2.5), 0.0f, "initial silence");
    for (int frame = 0; frame < 40; ++frame)
    {
        // Different signals and reversed write order catch stereo coupling.
        buffer.writeSample(1, -static_cast<float>(frame + 1));
        buffer.writeSample(0, static_cast<float>(frame + 1));
        for (double delay = 0.0; delay <= 4.0; delay += 0.25)
        {
            const auto age = static_cast<int>(std::floor(delay));
            const auto fraction = static_cast<float>(delay - age);
            const auto newer = frame >= age ? static_cast<float>(frame - age + 1) : 0.0f;
            const auto older = frame > age ? static_cast<float>(frame - age) : 0.0f;
            const auto expected = newer * (1.0f - fraction) + older * fraction;
            near(buffer.readDelayedSample(0, delay), expected, "fractional/wrapped read");
            near(buffer.readDelayedSample(1, delay), -expected, "stereo isolation");
        }
    }
    near(buffer.readDelayedSample(0, -1.0), 40.0f, "negative delay clamp");
    near(buffer.readDelayedSample(0, 99.0), 36.0f, "maximum delay clamp");
    near(buffer.readDelayedSample(2, 0.0), 0.0f, "invalid channel");
    near(buffer.readDelayedSample(0, std::numeric_limits<double>::quiet_NaN()), 0.0f, "invalid delay");
    buffer.reset();
    near(buffer.readDelayedSample(0, 4.0), 0.0f, "reset old history");
    buffer.writeSample(0, 2.0f);
    near(buffer.readDelayedSample(0, 1.0e-20), 2.0f, "tiny fractional delay at seam");
    near(buffer.readDelayedSample(0, 0.5), 1.0f, "interpolation across slot zero");

    buffer.prepare(5.0, 0.5, 2); // A fractional maximum of 2.5 samples.
    for (int i = 1; i <= 20; ++i)
        buffer.writeSample(0, static_cast<float>(i));
    near(buffer.readDelayedSample(0, 2.5), 17.5f, "fractional maximum");
    near(buffer.readDelayedSample(0, 50.0), 17.5f, "fractional maximum clamp");

    for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        buffer.prepare(rate, 10.0, 2);
        const auto history = static_cast<int>(rate * 10.0);
        for (int frame = 0; frame <= history + 7; ++frame)
        {
            buffer.writeSample(0, static_cast<float>(frame));
            buffer.writeSample(1, -static_cast<float>(frame));
        }
        near(buffer.readDelayedSample(0, history), 7.0f, "ten-second history");
        near(buffer.readDelayedSample(1, history), -7.0f, "ten-second stereo history");
    }
}

// Independent expected values at the five documented MEMORY knots, in seconds.
constexpr double delays[5][4] {
    { 0.100, 0.195, 0.320, 0.465 },
    { 0.300, 0.585, 0.960, 1.395 },
    { 0.800, 1.560, 2.560, 3.720 },
    { 1.400, 2.730, 4.480, 6.510 },
    { 2.000, 3.900, 6.400, 9.300 }
};
constexpr float pans[] { -0.25f, 0.25f, -0.55f, 0.55f };
constexpr float levels[] { -12.0f, -15.0f, -18.0f, -21.0f };

float voiceGain(int voice, int channel)
{
    const auto angle = (pans[voice] + 1.0f) * juce::MathConstants<float>::pi / 4.0f;
    return std::pow(10.0f, levels[voice] / 20.0f)
         * (channel == 0 ? std::cos(angle) : std::sin(angle));
}

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

// First event is dual mono; after a complete ring wrap, a left-only event tests
// distinct stereo input and any unintended crossfeed. All other input is zero.
std::array<std::vector<float>, 2> render(double rate, float memory, float blend, int blockSize, float drift = 0.0f, float blur = 0.0f, bool reverse = false)
{
    GhostEngine engine;
    engine.setBlur(0.0f); // Preserve the pre-BLUR regression baseline.
    engine.setDrift(0.0f); // Preserve the pre-DRIFT regression baseline.
    engine.setDecay(0.0f); // Preserve the pre-feedback regression baseline.
    engine.setDrift(drift);
    engine.setBlur(blur);
    engine.setReverse(reverse);
    engine.setMemory(memory);
    engine.setMix(blend);
    engine.prepare(rate, blockSize, 2);
    const auto length = static_cast<int>(rate * 20.0);
    const auto secondEvent = static_cast<int>(rate * 10.2);
    std::array<std::vector<float>, 2> result {
        std::vector<float>(static_cast<size_t>(length)),
        std::vector<float>(static_cast<size_t>(length))
    };
    juce::AudioBuffer<float> block(2, blockSize);
    for (int offset = 0; offset < length; offset += blockSize)
    {
        const auto count = juce::jmin(blockSize, length - offset);
        block.setSize(2, count, false, false, true);
        block.clear();
        if (offset == 0)
        {
            block.setSample(0, 0, 1.0f);
            block.setSample(1, 0, 1.0f);
        }
        if (secondEvent >= offset && secondEvent < offset + count)
            block.setSample(0, secondEvent - offset, 1.0f);
        engine.processBlock(block);
        for (int channel = 0; channel < 2; ++channel)
            std::copy_n(block.getReadPointer(channel), count,
                        result[static_cast<size_t>(channel)].begin() + offset);
    }
    engine.reset();
    block.clear();
    engine.processBlock(block);
    near(block.getMagnitude(0, block.getNumSamples()), 0.0f, "engine reset");
    return result;
}

void testImpulse(double rate, int knot)
{
    const auto output = render(rate, static_cast<float>(knot) * 0.25f, 1.0f, 127);
    const auto secondEvent = static_cast<int>(rate * 10.2);
    for (int channel = 0; channel < 2; ++channel)
    {
        double total = 0.0;
        double expectedTotal = 0.0;
        for (const auto value : output[static_cast<size_t>(channel)])
        {
            require(std::isfinite(value) && std::abs(value) <= 0.5f, "finite, bounded wet output");
            total += std::abs(value);
        }
        near(output[static_cast<size_t>(channel)][0], 0.0f, "100% MIX removes direct input", 0.0f);
        for (int event = 0; event < 2; ++event)
            for (int voice = 0; voice < 4; ++voice)
            {
                const auto centre = static_cast<int>(std::round(delays[knot][voice] * rate))
                                  + (event == 0 ? 0 : secondEvent);
                float localSum = 0.0f;
                // Sum a small neighbourhood rather than demanding an exact
                // floating-point peak: fractional delays can span two samples.
                for (int i = centre - 2; i <= centre + 2; ++i)
                    localSum += output[static_cast<size_t>(channel)][static_cast<size_t>(i)];
                const auto expected = (event == 1 && channel == 1) ? 0.0f : voiceGain(voice, channel);
                near(localSum, expected, "four distinct voices: timing, gain, pan and ring wrap", 2.0e-4f);
                expectedTotal += expected;
            }
        near(static_cast<float>(total), static_cast<float>(expectedTotal), "no extra repeats/feedback", 5.0e-4f);
    }
}

void testVoiceTransitions(double rate)
{
    GhostBuffer history;
    history.prepare(rate, 10.0, 2);
    const auto count = static_cast<int>(rate * 10.0);
    for (int i = 0; i <= count; ++i)
    {
        history.writeSample(0, static_cast<float>(i) / static_cast<float>(count));
        history.writeSample(1, -static_cast<float>(i) / static_cast<float>(count));
    }
    GhostVoice voice;
    voice.prepare(rate, 10.0, 1.0, 0.0f, 0.0f);
    float left = 0.0f, right = 0.0f;
    voice.processSample(history, left, right);
    auto previous = left;
    voice.setDelaySeconds(9.0);
    const auto fadeSamples = static_cast<int>(rate * 0.1);
    for (int frame = 0; frame < fadeSamples * 4; ++frame)
    {
        // Rapid retargeting during the first fade, then settle at two seconds.
        if (frame < fadeSamples)
            voice.setDelaySeconds(frame % 2 == 0 ? 8.0 : 2.0);
        else
            voice.setDelaySeconds(2.0);
        left = right = 0.0f;
        voice.processSample(history, left, right);
        require(std::abs(left - previous) < 2.0f / static_cast<float>(fadeSamples),
                "MEMORY transitions cannot jump/restart audible read heads");
        near(right, -left, "voice preserves independent channel signs");
        previous = left;
    }
    near(left, 0.8f * std::sqrt(0.5f), "latest automation target reached", 2.0e-5f);
    voice.setDelaySeconds(7.0);
    voice.reset();
    left = right = 0.0f;
    voice.processSample(history, left, right);
    near(left, 0.3f * std::sqrt(0.5f), "voice reset cancels pending fade");
}

void testMixAndAutomation(double rate)
{
    GhostEngine engine;
    engine.setBlur(0.0f); // Preserve the pre-BLUR regression baseline.
    engine.setDrift(0.0f); // Preserve the pre-DRIFT regression baseline.
    engine.setDecay(0.0f); // Preserve the pre-feedback regression baseline.
    engine.setMix(0.0f);
    engine.setMemory(0.0f);
    engine.prepare(rate, 127, 2);
    juce::AudioBuffer<float> block(2, 127);
    // Full-scale, different-channel material; dry stays bit-identical while
    // MEMORY changes and the engine continually records more than ten seconds.
    for (int offset = 0; offset < static_cast<int>(rate * 10.5); offset += 127)
    {
        engine.setMemory(static_cast<float>((offset / 127) % 101) / 100.0f);
        for (int i = 0; i < 127; ++i)
        {
            block.setSample(0, i, 1.0f);
            block.setSample(1, i, -0.75f);
        }
        engine.processBlock(block);
        for (int i = 0; i < 127; ++i)
        {
            near(block.getSample(0, i), 1.0f, "bit-identical dry left", 0.0f);
            near(block.getSample(1, i), -0.75f, "bit-identical dry right", 0.0f);
        }
    }
    float wetLeft = 0.0f, wetRight = 0.0f;
    for (int voice = 0; voice < 4; ++voice)
    {
        wetLeft += voiceGain(voice, 0);
        wetRight += -0.75f * voiceGain(voice, 1);
    }
    engine.setMix(1.0f);
    juce::AudioBuffer<float> sample(2, 1);
    const auto rampSamples = static_cast<int>(rate * 0.05);
    for (int frame = 1; frame <= rampSamples; ++frame)
    {
        engine.setMix(1.0f); // Repeated block updates must not restart MIX.
        sample.setSample(0, 0, 1.0f);
        sample.setSample(1, 0, -0.75f);
        engine.processBlock(sample);
        const auto angle = static_cast<float>(frame) / static_cast<float>(rampSamples)
                         * juce::MathConstants<float>::halfPi;
        near(sample.getSample(0, 0), std::cos(angle) + wetLeft * std::sin(angle), "50 ms MIX ramp left", 2.0e-5f);
        near(sample.getSample(1, 0), -0.75f * std::cos(angle) + wetRight * std::sin(angle), "50 ms MIX ramp right", 2.0e-5f);
    }
    near(sample.getSample(0, 0), wetLeft, "fully wet, history recorded at zero MIX");

    // Changing MEMORY repeatedly on sustained full-scale material must not add
    // gain, jump, or leak channels, even when several fades are pending.
    for (int frame = 0; frame < static_cast<int>(rate * 0.5); ++frame)
    {
        engine.setMemory(frame % 2 == 0 ? 0.0f : 1.0f);
        sample.setSample(0, 0, 1.0f);
        sample.setSample(1, 0, -0.75f);
        engine.processBlock(sample);
        near(sample.getSample(0, 0), wetLeft, "bounded automated wet left");
        near(sample.getSample(1, 0), wetRight, "bounded automated wet right");
    }
    // Re-prepare must discard old audio and preserve the new parameter targets.
    engine.prepare(rate, 127, 2);
    sample.clear();
    engine.processBlock(sample);
    near(sample.getMagnitude(0, 1), 0.0f, "reprepare clears history");
}

struct TailStats
{
    double afterInitialEnergy = 0.0;
    std::array<double, 18> energy {};
    std::array<float, 18> peak {};
    float overallPeak = 0.0f;
};

TailStats runTail(double rate, float memoryAmount, float decayAmount, int seconds,
                  int blockSize, int channelMode = 0, float drift = 0.0f, float blur = 0.0f, bool reverse = false)
{
    GhostEngine engine;
    engine.setBlur(0.0f); // Preserve the pre-BLUR regression baseline.
    engine.setDrift(0.0f); // Preserve the pre-DRIFT regression baseline.
    engine.setDrift(drift);
    engine.setBlur(blur);
    engine.setReverse(reverse);
    engine.setMemory(memoryAmount);
    engine.setDecay(decayAmount);
    engine.setMix(1.0f);
    engine.prepare(rate, blockSize, 2);
    juce::AudioBuffer<float> block(2, blockSize);
    TailStats stats;
    const auto length = static_cast<int>(rate * seconds);
    const auto burstLength = static_cast<int>(rate * 0.1);
    for (int offset = 0; offset < length; offset += blockSize)
    {
        const auto count = juce::jmin(blockSize, length - offset);
        block.setSize(2, count, false, false, true);
        for (int i = 0; i < count; ++i)
        {
            const auto inBurst = offset + i < burstLength;
            block.setSample(0, i, inBurst && channelMode != 2 ? 0.5f : 0.0f);
            block.setSample(1, i, inBurst && channelMode != 1 ? -0.35f : 0.0f);
        }
        engine.processBlock(block);
        for (int i = 0; i < count; ++i)
        {
            const auto bin = static_cast<size_t>((offset + i) / static_cast<int>(rate * 10.0));
            for (int ch = 0; ch < 2; ++ch)
            {
                const auto value = block.getSample(ch, i);
                require(std::isfinite(value) && std::abs(value) < 0.5f, "long tail finite and bounded");
                if ((channelMode == 1 && ch == 1) || (channelMode == 2 && ch == 0))
                    near(value, 0.0f, "feedback has no cross-channel leakage", 0.0f);
                const auto power = static_cast<double>(value) * value / rate;
                stats.energy[bin] += power;
                stats.peak[bin] = juce::jmax(stats.peak[bin], std::abs(value));
                stats.overallPeak = juce::jmax(stats.overallPeak, std::abs(value));
                if (offset + i > static_cast<int>(rate * 4.0))
                    stats.afterInitialEnergy += power;
            }
        }
    }
    // Exercise reset after feedback has actually accumulated, with enough
    // silence to catch a stale feedback frame reappearing at any voice delay.
    engine.reset();
    for (int offset = 0; offset < static_cast<int>(rate * 10.1); offset += blockSize)
    {
        block.clear();
        engine.processBlock(block);
        near(block.getMagnitude(0, block.getNumSamples()), 0.0f, "reset clears return and ring", 0.0f);
    }
    return stats;
}

void testFeedbackRamp(double rate)
{
    GhostFeedback feedback;
    feedback.setDecay(0.0f);
    feedback.prepare(rate, 1.0, 1.0);
    feedback.capture(0.1f, -0.2f);
    float left = 0.0f, right = 0.0f;
    feedback.getReturn(left, right);
    near(left, 0.0f, "zero decay disables feedback", 0.0f);
    feedback.setDecay(1.0f);
    const auto frames = static_cast<int>(rate * 0.1);
    for (int frame = 1; frame <= frames; ++frame)
    {
        feedback.setDecay(1.0f);
        feedback.getReturn(left, right);
        const auto coefficient = 0.95 * static_cast<double>(frame) / frames;
        near(left, static_cast<float>(coefficient * std::tanh(0.1)), "100 ms DECAY ramp", 2.0e-6f);
        near(right, static_cast<float>(coefficient * std::tanh(-0.2)), "stereo DECAY ramp", 2.0e-6f);
    }
    feedback.setDecay(0.0f);
    for (int frame = 0; frame < frames; ++frame)
        feedback.getReturn(left, right);
    near(left, 0.0f, "DECAY ramp reaches exact zero", 0.0f);
    feedback.capture(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN());
    feedback.setDecay(1.0f);
    feedback.getReturn(left, right);
    near(left, 0.0f, "feedback infinity guard", 0.0f);
    near(right, 0.0f, "feedback NaN guard", 0.0f);
}

void testDecayStress(double rate, float drift = 0.0f, float blur = 0.0f, bool reverse = false)
{
    GhostEngine engine, dryEngine;
    engine.setReverse(reverse);
    dryEngine.setReverse(reverse);
    engine.setBlur(blur);
    dryEngine.setBlur(blur);
    engine.setDrift(drift);
    dryEngine.setDrift(drift);
    engine.setMix(1.0f);
    dryEngine.setMix(0.0f);
    engine.setMemory(1.0f);
    dryEngine.setMemory(1.0f);
    engine.setDecay(1.0f);
    dryEngine.setDecay(1.0f);
    engine.prepare(rate, 256, 2);
    dryEngine.prepare(rate, 256, 2);
    juce::AudioBuffer<float> block(2, 256), dryBlock(2, 256);
    float peak = 0.0f;
    // 60 seconds of worst-case correlated sustained input, initially with all
    // three knobs at 100%; then automate MEMORY and DECAY aggressively.
    for (int offset = 0; offset < static_cast<int>(rate * 60.0); offset += 256)
    {
        if (offset > static_cast<int>(rate * 30.0))
        {
            const auto control = ((offset / 256) % 17) < 8 ? 0.0f : 1.0f;
            engine.setMemory(control);
            dryEngine.setMemory(control);
            engine.setDecay(control);
            dryEngine.setDecay(control);
            if (reverse)
            {
                engine.setReverse(control > 0.5f);
                dryEngine.setReverse(control > 0.5f);
            }
            if (blur > 0.0f)
            {
                engine.setBlur(control);
                dryEngine.setBlur(control);
            }
            if (drift > 0.0f)
            {
                engine.setDrift(control);
                dryEngine.setDrift(control);
            }
        }
        for (int i = 0; i < 256; ++i)
        {
            block.setSample(0, i, 1.0f);
            block.setSample(1, i, -1.0f);
            dryBlock.setSample(0, i, 1.0f);
            dryBlock.setSample(1, i, -1.0f);
        }
        engine.processBlock(block);
        dryEngine.processBlock(dryBlock);
        for (int i = 0; i < 256; ++i)
        {
            near(dryBlock.getSample(0, i), 1.0f, "MIX zero unchanged at maximum feedback", 0.0f);
            near(dryBlock.getSample(1, i), -1.0f, "MIX zero unchanged stereo", 0.0f);
            for (int ch = 0; ch < 2; ++ch)
            {
                const auto value = block.getSample(ch, i);
                require(std::isfinite(value) && std::abs(value) < 1.0f, "sustained full-scale stress bounds");
                peak = juce::jmax(peak, std::abs(value));
            }
        }
    }
    // The hidden loop at MIX zero must match the audible loop when revealed.
    dryEngine.setMix(1.0f);
    for (int offset = 0; offset < static_cast<int>(rate * 0.2); offset += 256)
    {
        block.clear();
        dryBlock.clear();
        engine.processBlock(block);
        dryEngine.processBlock(dryBlock);
        if (offset > static_cast<int>(rate * 0.06))
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                    near(block.getSample(ch, i), dryBlock.getSample(ch, i), "feedback independent of MIX", 1.0e-6f);
    }
    // Fault injection must not contaminate either the immediate dry output or
    // history. Test both mix endpoints and an intermediate mix after settling.
    for (const float mixAmount : { 0.0f, 0.5f, 1.0f })
    {
        engine.setMix(mixAmount);
        engine.prepare(rate, 256, 2);
        block.clear();
        block.setSample(0, 0, std::numeric_limits<float>::quiet_NaN());
        block.setSample(0, 1, std::numeric_limits<float>::infinity());
        block.setSample(1, 0, -std::numeric_limits<float>::infinity());
        block.setSample(1, 1, 1.0e30f);
        block.setSample(1, 2, std::numeric_limits<float>::denorm_min());
        engine.setDecay(std::numeric_limits<float>::quiet_NaN());
        engine.setDrift(std::numeric_limits<float>::quiet_NaN());
        engine.setBlur(std::numeric_limits<float>::quiet_NaN());
        engine.setMemory(std::numeric_limits<float>::infinity());
        engine.setMix(std::numeric_limits<float>::quiet_NaN());
        engine.processBlock(block);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 256; ++i)
                near(block.getSample(ch, i), 0.0f, "invalid input containment");
    }
    std::cout << rate << " Hz DRIFT=" << drift << " BLUR=" << blur << " REVERSE=" << reverse << " sustained/automation stress peak=" << peak
              << " (" << 20.0f * std::log10(peak) << " dBFS)\n";
}

void testDecay()
{
    for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        testFeedbackRamp(rate);
        double previousEnergy = -1.0;
        for (const float decay : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            const auto stats = runTail(rate, 0.5f, decay, 60, 127);
            if (decay == 0.0f)
                require(stats.afterInitialEnergy == 0.0, "DECAY zero: only original four arrivals");
            else
                require(stats.afterInitialEnergy > juce::jmax(1.0e-10, previousEnergy * 1.2),
                        "increasing DECAY extends post-arrival energy substantially");
            previousEnergy = stats.afterInitialEnergy;
            std::cout << rate << " Hz DECAY=" << decay * 100.0f
                      << "% post-4s energy=" << stats.afterInitialEnergy << '\n';
        }
        const auto minimum = runTail(rate, 0.0f, 1.0f, 60, 64, 1);
        require(minimum.energy[5] < minimum.energy[0] * 1.0e-4, "short MEMORY maximum DECAY dies away");
        const auto maximum = runTail(rate, 1.0f, 1.0f, 180, 512);
        require(maximum.energy[5] > 1.0e-10, "maximum DECAY persists beyond 50 seconds");
        require(maximum.energy[17] < maximum.energy[5] * 0.2, "maximum DECAY trends downward by 180 seconds");
        require(maximum.peak[17] < maximum.peak[0] * 0.1f, "no exponential growth in maximum tail");
        const auto rightOnly = runTail(rate, 1.0f, 1.0f, 60, 127, 2);
        require(rightOnly.afterInitialEnergy > 0.0, "right-only feedback persists");
        // Same run with different host blocks must have the same tail metrics.
        const auto alternate = runTail(rate, 0.0f, 1.0f, 60, 512, 1);
        near(minimum.overallPeak, alternate.overallPeak, "feedback block-size peak independence");
        require(std::abs(minimum.afterInitialEnergy - alternate.afterInitialEnergy) < 1.0e-10,
                "feedback block-size energy independence");
        std::cout << rate << " Hz 180s max-memory/max-decay burst peak=" << maximum.overallPeak
                  << "; energy 50-60s=" << maximum.energy[5]
                  << ", 170-180s=" << maximum.energy[17]
                  << "; final peak=" << maximum.peak[17] << '\n';
        testDecayStress(rate);
    }
}

void testVoiceDrift(double rate)
{
    GhostBuffer history;
    history.prepare(rate, 10.0, 2);
    constexpr double lfoRates[] { 0.07, 0.11, 0.17, 0.23 };
    constexpr double phases[] { 0.0, 0.23, 0.51, 0.79 };
    std::array<double, 4> trajectorySamples {};
    for (int voiceIndex = 0; voiceIndex < 4; ++voiceIndex)
    {
        for (const float drift : { 0.0f, 0.25f, 0.5f, 1.0f })
        {
            GhostVoice voice;
            voice.setDrift(drift);
            const auto baseSeconds = delays[0][voiceIndex];
            voice.prepare(rate, 10.0, baseSeconds, 0.0f, 0.0f, lfoRates[voiceIndex], phases[voiceIndex]);
            double previousDelay = 0.0;
            double lowestCents = 1.0e6, highestCents = -1.0e6;
            const auto frames = static_cast<int>(rate * 16.0); // More than a full cycle of every LFO.
            for (int frame = 0; frame < frames; ++frame)
            {
                float left = 0.0f, right = 0.0f;
                voice.processSample(history, left, right);
                const auto read = voice.getReadDelaySamples();
                require(std::isfinite(read) && read >= 0.0 && read <= rate * 10.0,
                        "DRIFT read within prepared history at minimum MEMORY");
                require(std::isfinite(left) && std::isfinite(right), "voice output finite");
                if (frame > 0)
                {
                    // Independent pitch measurement from the actual read-head
                    // velocity, not from a target or a copy of the LFO formula.
                    const auto speed = 1.0 - (read - previousDelay);
                    require(speed > 0.96 && speed < 1.04, "bounded forward playback speed");
                    const auto cents = 1200.0 * std::log2(speed);
                    lowestCents = juce::jmin(lowestCents, cents);
                    highestCents = juce::jmax(highestCents, cents);
                }
                previousDelay = read;
                if (frame == static_cast<int>(rate) && drift == 1.0f)
                    trajectorySamples[static_cast<size_t>(voiceIndex)] = read / rate - baseSeconds;
            }
            const auto expectedDepth = 50.0 * static_cast<double>(drift) * drift;
            if (drift == 0.0f)
                require(std::abs(lowestCents) < 1.0e-7 && std::abs(highestCents) < 1.0e-7,
                        "zero DRIFT has no playback-speed movement");
            else
            {
                require(std::abs(lowestCents + expectedDepth) < 0.05,
                        "read velocity achieves requested detuning");
                require(highestCents > expectedDepth * 0.96 && highestCents < expectedDepth + 0.05,
                        "positive detuning remains close to intended cents");
            }
            if (voiceIndex == 0)
                std::cout << rate << " Hz DRIFT=" << drift * 100.0f << "% measured cents="
                          << lowestCents << " to " << highestCents << '\n';
        }
    }
    for (size_t a = 0; a < trajectorySamples.size(); ++a)
        for (size_t b = a + 1; b < trajectorySamples.size(); ++b)
            require(std::abs(trajectorySamples[a] - trajectorySamples[b]) > 1.0e-4,
                    "four distinct voice trajectories");

    GhostVoice voice;
    voice.prepare(rate, 10.0, 0.1, 0.0f, 0.0f, 0.07, 0.23);
    // Zero amount must still advance the oscillator. Nonzero initial phase also
    // exposes abrupt depth changes that a zero-crossing-only test could miss.
    for (int i = 0; i < 100; ++i)
    {
        float left = 0.0f, right = 0.0f;
        voice.processSample(history, left, right);
    }
    const auto zeroPhase = voice.getPhaseCycles();
    require(zeroPhase != 0.23, "phase runs at zero DRIFT");
    const auto rampFrames = static_cast<int>(rate * 0.15);
    double previousRead = voice.getReadDelaySamples();
    for (const float target : { 1.0f, 0.0f })
    {
        const auto beforePhase = voice.getPhaseCycles();
        voice.setDrift(target);
        require(voice.getPhaseCycles() == beforePhase, "DRIFT must not reset phase");
        for (int frame = 0; frame < rampFrames; ++frame)
        {
            voice.setDrift(target); // Constant automation must not restart ramp.
            float left = 0.0f, right = 0.0f;
            voice.processSample(history, left, right);
            const auto read = voice.getReadDelaySamples();
            require(std::abs(read - previousRead) < 0.5, "DRIFT automation has no read-position jump");
            previousRead = read;
        }
        require(target != 0.0f || voice.getDepthSamples() == 0.0, "150 ms ramp reaches exact zero");
    }
    voice.setDrift(1.0f);
    // Even artificial base delays near either boundary must not read outside
    // storage. Both crossfade taps use the same uninterrupted modulation phase.
    for (int frame = 0; frame < static_cast<int>(rate * 0.8); ++frame)
    {
        if (frame % 97 == 0)
            voice.setDelaySeconds(frame % 194 == 0 ? 0.0001 : 9.9999);
        const auto oldPhase = voice.getPhaseCycles();
        float left = 0.0f, right = 0.0f;
        voice.processSample(history, left, right);
        auto advance = voice.getPhaseCycles() - oldPhase;
        if (advance < 0.0) advance += 1.0;
        require(std::abs(advance - 0.07 / rate) < 1.0e-12, "MEMORY preserves oscillator phase");
        for (const auto position : { voice.getReadDelaySamples(), voice.getDestinationReadDelaySamples() })
            require(position >= 0.0 && position <= rate * 10.0, "both crossfade heads bounded");
    }
    voice.reset();
    require(voice.getPhaseCycles() == 0.23, "reset restores deterministic phase");

    // Audible signal-path check: anti-phase stereo remains coherent while the
    // modulated voice differs from its unmodulated counterpart on a 220 Hz tone.
    GhostVoice moving, stationary;
    moving.setDrift(0.5f);
    moving.prepare(rate, 10.0, 0.1, 0.0f, 0.0f, 0.11, 0.23);
    stationary.prepare(rate, 10.0, 0.1, 0.0f, 0.0f, 0.11, 0.23);
    history.reset();
    double differenceEnergy = 0.0;
    for (int frame = 0; frame < static_cast<int>(rate * 2.0); ++frame)
    {
        const auto input = static_cast<float>(0.5 * std::sin(juce::MathConstants<double>::twoPi * 220.0 * frame / rate));
        history.writeSample(0, input);
        history.writeSample(1, -input);
        float left = 0.0f, right = 0.0f, referenceLeft = 0.0f, referenceRight = 0.0f;
        moving.processSample(history, left, right);
        stationary.processSample(history, referenceLeft, referenceRight);
        near(right, -left, "DRIFT maintains coherent stereo", 1.0e-6f);
        differenceEnergy += static_cast<double>(left - referenceLeft) * (left - referenceLeft) / rate;
    }
    require(differenceEnergy > 0.001, "modulation actually affects audio samples");
}

void testDrift()
{
    for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        testVoiceDrift(rate);
        const auto small = render(rate, 0.5f, 1.0f, 64, 1.0f);
        const auto large = render(rate, 0.5f, 1.0f, 512, 1.0f);
        for (size_t ch = 0; ch < small.size(); ++ch)
            for (size_t i = 0; i < small[ch].size(); ++i)
                near(small[ch][i], large[ch][i], "DRIFT block-size independence", 1.0e-6f);
        const auto minimum = runTail(rate, 0.0f, 1.0f, 60, 64, 1, 1.0f);
        require(minimum.energy[5] < minimum.energy[0] * 1.0e-4, "short-memory drifting tail decays");
        const auto maximum = runTail(rate, 1.0f, 1.0f, 180, 512, 0, 1.0f);
        require(maximum.energy[5] > 1.0e-10, "drifting long tail persists");
        require(maximum.energy[17] < maximum.energy[5] * 0.2, "drifting maximum tail trends downward");
        require(maximum.peak[17] < maximum.peak[0] * 0.1f, "drifting feedback cannot grow exponentially");
        const auto right = runTail(rate, 1.0f, 1.0f, 60, 127, 2, 1.0f);
        require(right.afterInitialEnergy > 0.0, "right-only drifting feedback persists");
        std::cout << rate << " Hz DRIFT=100% 180s burst peak=" << maximum.overallPeak
                  << "; energy 50-60s=" << maximum.energy[5]
                  << ", 170-180s=" << maximum.energy[17]
                  << "; final peak=" << maximum.peak[17] << '\n';
        testDecayStress(rate, 1.0f);
    }
}

struct BlurImpulseStats
{
    double energy = 0.0;
    double centroid = 0.0;
    double lateEnergy = 0.0;
    float peak = 0.0f;
};

BlurImpulseStats blurImpulse(double rate, float amount)
{
    juce::ScopedNoDenormals noDenormals;
    GhostBlur blur;
    blur.setAmount(amount);
    blur.prepare(rate);
    BlurImpulseStats stats;
    for (int frame = 0; frame < static_cast<int>(rate * 0.5); ++frame)
    {
        auto left = frame == 0 ? 1.0f : 0.0f;
        float right = 0.0f;
        blur.beginFrame();
        blur.filterVoice(0, left, right);
        blur.diffuse(left, right);
        require(std::isfinite(left) && std::abs(left) <= 1.0f, "BLUR impulse finite, no gain spike");
        near(right, 0.0f, "diffusion does not crossfeed", 0.0f);
        const auto power = static_cast<double>(left) * left;
        stats.energy += power;
        stats.centroid += power * frame / rate;
        if (frame > static_cast<int>(rate * 0.003)) stats.lateEnergy += power;
        stats.peak = juce::jmax(stats.peak, std::abs(left));
        if (amount == 0.0f)
            near(left, frame == 0 ? 1.0f : 0.0f, "BLUR zero exact transient bypass", 0.0f);
    }
    require(stats.energy > 0.01 && stats.energy <= 1.00001, "BLUR does not silence or amplify impulse energy");
    stats.centroid /= stats.energy;
    blur.reset();
    for (int frame = 0; frame < static_cast<int>(rate * 0.3); ++frame)
    {
        float left = 0.0f, right = 0.0f;
        blur.beginFrame();
        blur.filterVoice(0, left, right);
        blur.diffuse(left, right);
        near(left, 0.0f, "BLUR reset clears diffusion", 0.0f);
    }
    return stats;
}

double blurToneGain(double rate, float amount, double frequency, bool diffusion)
{
    juce::ScopedNoDenormals noDenormals;
    GhostBlur blur;
    blur.setAmount(amount);
    blur.prepare(rate);
    double inputEnergy = 0.0, outputEnergy = 0.0;
    for (int frame = 0; frame < static_cast<int>(rate); ++frame)
    {
        const auto input = static_cast<float>(0.5 * std::sin(juce::MathConstants<double>::twoPi * frequency * frame / rate));
        float left = input, right = -input;
        blur.beginFrame();
        blur.filterVoice(0, left, right);
        if (diffusion) blur.diffuse(left, right);
        require(std::isfinite(left) && std::isfinite(right), "BLUR tone finite");
        if (frame > static_cast<int>(rate * 0.5))
        {
            inputEnergy += static_cast<double>(input) * input;
            outputEnergy += static_cast<double>(left) * left;
        }
    }
    return std::sqrt(outputEnergy / inputEnergy);
}

void testBlurModule(double rate)
{
    double previousCentroid = -1.0, previousHighGain = 2.0;
    for (const float amount : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        const auto stats = blurImpulse(rate, amount);
        const auto highGain = blurToneGain(rate, amount, 10000.0, false);
        require(highGain < previousHighGain, "increasing BLUR reduces voice high-frequency energy");
        require(stats.centroid > previousCentroid, "increasing BLUR increases temporal spread");
        if (amount > 0.0f)
        {
            require(stats.lateEnergy > 0.0, "BLUR has time smear beyond filtering alone");
            const auto cutoff = 20000.0 * std::pow(0.1, static_cast<double>(amount));
            const auto cutoffGain = blurToneGain(rate, amount, cutoff, false);
            require(cutoffGain > 0.70 && cutoffGain < 0.715, "digital cutoff matches -3 dB mapping");
        }
        const auto bassGain = blurToneGain(rate, amount, 100.0, true);
        require(bassGain > 0.6, "BLUR retains useful low-frequency body");
        previousHighGain = highGain;
        previousCentroid = stats.centroid;
        std::cout << rate << " Hz BLUR=" << amount * 100.0f << "%: impulse peak=" << stats.peak
                  << ", energy centroid ms=" << stats.centroid * 1000.0
                  << ", voice 10kHz gain=" << highGain << '\n';
    }

    juce::ScopedNoDenormals noDenormals;
    GhostBlur blur;
    blur.prepare(rate);
    // A deterministic broadband burst exercises decorrelation and transient
    // overshoot. This also serves as a short clap/percussion-like stimulus.
    uint32_t noiseState = 0x19ab2468u;
    double differenceEnergy = 0.0, leftEnergy = 0.0, monoEnergy = 0.0;
    blur.setAmount(1.0f);
    blur.reset();
    for (int frame = 0; frame < static_cast<int>(rate * 2.0); ++frame)
    {
        noiseState = noiseState * 1664525u + 1013904223u;
        const auto input = frame < static_cast<int>(rate * 0.1)
            ? static_cast<float>(static_cast<double>(noiseState) / 4294967295.0 - 0.5) : 0.0f;
        float left = input, right = input;
        blur.beginFrame();
        blur.filterVoice(0, left, right);
        blur.diffuse(left, right);
        require(std::isfinite(left) && std::isfinite(right) && std::abs(left) < 1.0f
                && std::abs(right) < 1.0f, "broadband BLUR finite, bounded");
        differenceEnergy += static_cast<double>(left - right) * (left - right);
        leftEnergy += static_cast<double>(left) * left;
        monoEnergy += static_cast<double>(0.5f * (left + right)) * (0.5f * (left + right));
    }
    require(differenceEnergy > leftEnergy * 0.05, "short unequal all-passes decorrelate stereo");
    require(monoEnergy > leftEnergy * 0.2, "mono sum retains meaningful signal");

    // All-pass phase spreading can raise a rectangular burst's peak even while
    // preserving energy. Investigate that separately from the long-loop peak;
    // don't mask it with a limiter or assume unity magnitude means unity peak.
    blur.reset();
    double burstInputEnergy = 0.0, burstOutputEnergy = 0.0;
    float burstPeak = 0.0f;
    for (int frame = 0; frame < static_cast<int>(rate); ++frame)
    {
        const auto input = frame < static_cast<int>(rate * 0.1) ? 0.5f : 0.0f;
        float left = input, right = 0.0f;
        blur.beginFrame();
        blur.filterVoice(0, left, right);
        blur.diffuse(left, right);
        burstInputEnergy += static_cast<double>(input) * input;
        burstOutputEnergy += static_cast<double>(left) * left;
        burstPeak = juce::jmax(burstPeak, std::abs(left));
    }
    require(burstOutputEnergy <= burstInputEnergy * 1.00001, "diffused burst adds no energy");
    require(burstPeak < 1.0f, "isolated diffuser burst overshoot stays bounded");
    std::cout << rate << " Hz isolated BLUR burst: input peak=0.5, output peak=" << burstPeak
              << ", energy ratio=" << burstOutputEnergy / burstInputEnergy << '\n';

    // Directly test the first frame of an automation jump with non-DC content.
    blur.setAmount(0.0f);
    blur.reset();
    blur.setAmount(1.0f);
    float firstLeft = 1.0f, firstRight = 0.0f;
    blur.beginFrame();
    blur.filterVoice(0, firstLeft, firstRight);
    blur.diffuse(firstLeft, firstRight);
    require(firstLeft > 0.999f, "BLUR target jump does not instantly switch processing");

    // At both parameter extremes the steady-state DC gain is unity. Warm all
    // states before changing the knob to reveal state-switching discontinuities.
    blur.setAmount(0.0f);
    blur.reset();
    for (int frame = 0; frame < static_cast<int>(rate * 0.5); ++frame)
    {
        float left = 0.5f, right = -0.25f;
        blur.beginFrame();
        blur.filterVoice(0, left, right);
        blur.diffuse(left, right);
    }
    for (const float target : { 1.0f, 0.0f })
    {
        blur.setAmount(target);
        for (int frame = 0; frame < static_cast<int>(rate * 0.16); ++frame)
        {
            blur.setAmount(target);
            float left = 0.5f, right = -0.25f;
            blur.beginFrame();
            blur.filterVoice(0, left, right);
            blur.diffuse(left, right);
            near(left, 0.5f, "BLUR automation keeps steady input continuous", 1.0e-5f);
            near(right, -0.25f, "BLUR automation stereo continuity", 1.0e-5f);
        }
    }
    // After 150 ms back at zero, arbitrary samples must bypass exactly, even
    // though the diffuser still contains previous nonzero audio.
    float left = 0.123f, right = -0.456f;
    blur.beginFrame();
    blur.filterVoice(0, left, right);
    blur.diffuse(left, right);
    near(left, 0.123f, "BLUR reaches transparent bypass after ramp", 0.0f);
    near(right, -0.456f, "BLUR transparent right after ramp", 0.0f);
}

void testBlur()
{
    for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        testBlurModule(rate);
        const auto small = render(rate, 0.5f, 1.0f, 64, 1.0f, 1.0f);
        const auto large = render(rate, 0.5f, 1.0f, 512, 1.0f, 1.0f);
        for (size_t ch = 0; ch < small.size(); ++ch)
            for (size_t i = 0; i < small[ch].size(); ++i)
                near(small[ch][i], large[ch][i], "BLUR block-size independence", 1.0e-6f);
        const auto minimum = runTail(rate, 0.0f, 1.0f, 60, 64, 1, 1.0f, 1.0f);
        require(minimum.energy[5] < minimum.energy[0] * 1.0e-4, "short-memory all-max BLUR tail decays");
        const auto maximum = runTail(rate, 1.0f, 1.0f, 180, 512, 0, 1.0f, 1.0f);
        require(maximum.energy[5] > 1.0e-10, "BLUR retains long feedback persistence");
        require(maximum.energy[17] < maximum.energy[5] * 0.2, "all-max BLUR tail trends downward");
        require(maximum.peak[17] < maximum.peak[0] * 0.1f, "all-max BLUR tail has no runaway growth");
        const auto right = runTail(rate, 1.0f, 1.0f, 60, 127, 2, 1.0f, 1.0f);
        require(right.afterInitialEnergy > 0.0, "right-only blurred feedback persists");
        std::cout << rate << " Hz BLUR=100% 180s all-max burst peak=" << maximum.overallPeak
                  << " (" << 20.0f * std::log10(maximum.overallPeak) << " dBFS)"
                  << "; energy 50-60s=" << maximum.energy[5]
                  << ", 170-180s=" << maximum.energy[17]
                  << "; final peak=" << maximum.peak[17] << '\n';
        testDecayStress(rate, 1.0f, 1.0f);
    }
}

void testReverseHistory(double rate)
{
    GhostBuffer history;
    history.prepare(rate, 10.0, 2);
    require(history.getValidSamples(0) == 0, "new history is empty");
    require(!history.isDelayRecorded(0, 0.0), "empty history rejects zero delay");
    history.writeSample(0, 0.5f);
    require(history.getValidSamples(0) == 1 && history.getValidSamples(1) == 0,
            "valid history is counted independently per channel");
    require(history.isDelayRecorded(0, 0.0) && !history.isDelayRecorded(0, 0.5),
            "fractional read requires older recorded neighbour");
    history.reset();
    constexpr GhostReverseSettings configurations[] {
        { 0.20, 0.15, 0.50 }, { 0.39, 0.25, 0.80 },
        { 0.64, 0.40, 1.20 }, { 0.93, 0.60, 1.80 }
    };
    constexpr double lfoRates[] { 0.07, 0.11, 0.17, 0.23 };
    for (int voiceIndex = 0; voiceIndex < 4; ++voiceIndex)
        for (const double memoryDepth : { 0.5, 4.0, 10.0 })
        {
            history.reset();
            GhostReverse reverse;
            const auto guard = rate * (1.0 - std::exp2(-50.0 / 1200.0))
                             / (juce::MathConstants<double>::twoPi * lfoRates[voiceIndex]);
            reverse.prepare(rate, guard, configurations[voiceIndex]);
            reverse.setDelaySamples(rate * memoryDepth * configurations[voiceIndex].memoryRatio);
            float left = 0.0f, right = 0.0f;
            reverse.processSample(history, 0.0, left, right);
            require(!reverse.hasRegion(), "reverse waits for recorded history");
            near(left, 0.0f, "reverse startup silence", 0.0f);
            int decreasingPairs = 0;
            bool previousStable = false;
            float previousLeft = 0.0f;
            double previousSourceTime = 0.0;
            for (int frame = 0; frame < static_cast<int>(rate * 16.0); ++frame)
            {
                // Asymmetric known increasing sequence: every genuine backward
                // read outside a handover must locally DECREASE, including wraps.
                const auto input = static_cast<float>(frame / (rate * 32.0));
                history.writeSample(0, input);
                history.writeSample(1, -0.5f * input);
                const auto wasFading = reverse.isCrossfading();
                reverse.processSample(history, 0.0, left, right);
                require(std::isfinite(left) && std::isfinite(right), "reverse output finite");
                near(right, -0.5f * left, "reverse stereo timing coherence");
                for (const auto position : { reverse.getReadDelaySamples(), reverse.getIncomingReadDelaySamples() })
                    if (position >= 0.0)
                    {
                        require(history.isDelayRecorded(0, position) && history.isDelayRecorded(1, position),
                                "both reverse heads only read valid, bounded history");
                    }
                const auto stable = reverse.hasRegion() && !wasFading && !reverse.isCrossfading();
                const auto sourceTime = frame - reverse.getReadDelaySamples();
                if (stable && previousStable)
                {
                    require(left < previousLeft, "actual reverse samples must decrease");
                    require(std::abs((sourceTime - previousSourceTime) + 1.0) < 1.0e-7,
                            "reverse source moves -1 sample per output frame");
                    ++decreasingPairs;
                }
                previousStable = stable;
                previousLeft = left;
                previousSourceTime = sourceTime;
            }
            require(decreasingPairs > static_cast<int>(rate), "enough actual backward samples observed");
            require(reverse.getRejectedReads() == 0, "reverse cannot rely on read guards to conceal invalid scheduling");
            require(history.getValidSamples(0) == static_cast<int>(rate * 10.0) + 1,
                    "history count saturates at capacity");
            history.reset();
            reverse.reset();
            reverse.processSample(history, 0.0, left, right);
            near(left, 0.0f, "reset enabled reverse waits again", 0.0f);
        }
}

void testReverseClicks(double rate)
{
    GhostBuffer history;
    history.prepare(rate, 10.0, 2);
    GhostVoice voice;
    voice.setDrift(1.0f);
    voice.prepare(rate, 10.0, 0.1, 0.0f, 0.0f, 0.07, 0.23);
    float previous = 0.0f, peakStep = 0.0f;
    for (int frame = 0; frame < static_cast<int>(rate * 14.0); ++frame)
    {
        const auto t = frame / rate;
        const auto input = static_cast<float>(0.25 * std::sin(juce::MathConstants<double>::twoPi * 5.0 * t)
                                           + 0.1 * std::sin(juce::MathConstants<double>::twoPi * 1.3 * t));
        history.writeSample(0, input);
        history.writeSample(1, -input);
        if (frame == static_cast<int>(rate * 2.0) || frame == static_cast<int>(rate * 9.0))
        {
            const auto phase = voice.getPhaseCycles();
            voice.setReverse(true);
            require(voice.getPhaseCycles() == phase, "mode toggle does not reset DRIFT");
        }
        if (frame == static_cast<int>(rate * 8.0) || frame == static_cast<int>(rate * 12.0))
            voice.setReverse(false);
        if (frame == static_cast<int>(rate * 4.0)) voice.setDelaySeconds(2.0);
        if (frame == static_cast<int>(rate * 6.0)) voice.setDelaySeconds(0.1);
        float left = 0.0f, right = 0.0f;
        voice.processSample(history, left, right);
        peakStep = juce::jmax(peakStep, std::abs(left - previous));
        near(right, -left, "reverse mode transition preserves stereo");
        require(std::isfinite(left) && std::abs(left) < 0.4f, "mode/region/Memory transitions bounded");
        previous = left;
    }
    require(peakStep < 0.003f, "no large discontinuity at region restart, toggle, or MEMORY change");
    std::cout << rate << " Hz reverse click-test maximum sample step=" << peakStep << '\n';
}

void testReverseDrift(double rate)
{
    GhostBuffer history;
    history.prepare(rate, 10.0, 2);
    // Prefill so this measures the actual reverse trajectory, not startup silence.
    for (int i = 0; i < static_cast<int>(rate * 10.0) + 1; ++i)
    {
        history.writeSample(0, 0.0f);
        history.writeSample(1, 0.0f);
    }
    GhostReverse reverse;
    const auto q = 1.0 - std::exp2(-50.0 / 1200.0);
    const auto amplitude = rate * q / (juce::MathConstants<double>::twoPi * 0.07);
    reverse.prepare(rate, amplitude, { 0.20, 0.15, 0.50 });
    reverse.setDelaySamples(rate * 2.0);
    double previousRead = 0.0, minCents = 1.0e9, maxCents = -1.0e9;
    bool previousStable = false;
    for (int frame = 0; frame < static_cast<int>(rate * 16.0); ++frame)
    {
        history.writeSample(0, 0.0f);
        history.writeSample(1, 0.0f);
        const auto offset = amplitude * std::sin(juce::MathConstants<double>::twoPi * 0.07 * frame / rate);
        const auto wasFading = reverse.isCrossfading();
        float left = 0.0f, right = 0.0f;
        reverse.processSample(history, offset, left, right);
        const auto read = reverse.getReadDelaySamples();
        const auto stable = reverse.hasRegion() && !wasFading && !reverse.isCrossfading();
        if (stable && previousStable)
        {
            const auto velocity = 1.0 - (read - previousRead);
            require(velocity < -0.96 && velocity > -1.04, "DRIFT maintains reverse direction");
            const auto cents = 1200.0 * std::log2(-velocity);
            minCents = juce::jmin(minCents, cents);
            maxCents = juce::jmax(maxCents, cents);
        }
        previousRead = read;
        previousStable = stable;
    }
    require(minCents < -49.0 && maxCents > 47.5, "reverse DRIFT retains intended pitch depth");
    require(reverse.getRejectedReads() == 0, "reverse DRIFT never leaves safe region");
}

void testReverseMusical(double rate)
{
    for (int stimulus = 0; stimulus < 4; ++stimulus)
    {
        GhostEngine engine;
        engine.setReverse(true);
        engine.setMemory(0.0f);
        engine.setDecay(1.0f);
        engine.setDrift(1.0f);
        engine.setBlur(1.0f);
        engine.setMix(1.0f);
        engine.prepare(rate, 127, 2);
        juce::AudioBuffer<float> block(2, 127);
        double energy = 0.0;
        for (int offset = 0; offset < static_cast<int>(rate * 12.0); offset += 127)
        {
            for (int i = 0; i < 127; ++i)
            {
                const auto t = (offset + i) / rate;
                double value = 0.0;
                if (stimulus == 0 && t < 2.0) // Speech-like syllables/formant tones, not recorded speech.
                {
                    const auto envelope = std::pow(std::sin(juce::MathConstants<double>::pi * 3.0 * t), 2.0);
                    value = envelope * (0.22 * std::sin(juce::MathConstants<double>::twoPi * 130.0 * t)
                                      + 0.1 * std::sin(juce::MathConstants<double>::twoPi * 710.0 * t)
                                      + 0.06 * std::sin(juce::MathConstants<double>::twoPi * 1230.0 * t));
                }
                if (stimulus == 1 && offset + i == static_cast<int>(rate * 2.0)) value = 0.5;
                if (stimulus == 2 && t < 4.0) value = 0.4 * std::sin(juce::MathConstants<double>::twoPi * 220.0 * t);
                if (stimulus == 3 && t < 6.0 && (offset + i) % static_cast<int>(rate * 0.4) == 0) value = 0.5;
                block.setSample(0, i, static_cast<float>(value));
                block.setSample(1, i, static_cast<float>(-0.7 * value));
            }
            engine.processBlock(block);
            for (int i = 0; i < 127; ++i)
                for (int ch = 0; ch < 2; ++ch)
                {
                    const auto value = block.getSample(ch, i);
                    require(std::isfinite(value) && std::abs(value) < 1.0f, "reverse musical stimulus stable");
                    energy += static_cast<double>(value) * value / rate;
                }
        }
        require(energy > 1.0e-10, "reverse musical stimulus creates nonzero ghosts");
    }
}

void testReverse()
{
    for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        testReverseHistory(rate);
        testReverseClicks(rate);
        testReverseDrift(rate);
        testReverseMusical(rate);
        const auto small = render(rate, 0.5f, 1.0f, 64, 1.0f, 1.0f, true);
        const auto large = render(rate, 0.5f, 1.0f, 512, 1.0f, 1.0f, true);
        for (size_t ch = 0; ch < small.size(); ++ch)
            for (size_t i = 0; i < small[ch].size(); ++i)
                near(small[ch][i], large[ch][i], "reverse block-size independence", 1.0e-6f);
        const auto minimum = runTail(rate, 0.0f, 1.0f, 60, 64, 1, 1.0f, 1.0f, true);
        require(minimum.energy[5] < juce::jmax(1.0e-12, minimum.energy[0] * 0.01), "short reverse tail decays");
        const auto maximum = runTail(rate, 1.0f, 1.0f, 180, 512, 0, 1.0f, 1.0f, true);
        require(maximum.energy[5] > 1.0e-12, "maximum reverse tail retains persistence");
        require(maximum.energy[17] < maximum.energy[5] * 0.25, "maximum reverse tail trends downward");
        require(maximum.peak[17] < maximum.peak[0] * 0.1f, "maximum reverse tail cannot grow");
        const auto right = runTail(rate, 1.0f, 1.0f, 60, 127, 2, 1.0f, 1.0f, true);
        require(right.afterInitialEnergy > 0.0, "right-only reversed feedback persists");
        std::cout << rate << " Hz REVERSE all-max 180s peak=" << maximum.overallPeak
                  << " (" << 20.0f * std::log10(maximum.overallPeak) << " dBFS)"
                  << "; energy 50-60s=" << maximum.energy[5]
                  << ", 170-180s=" << maximum.energy[17]
                  << "; ratio=" << maximum.energy[17] / maximum.energy[5]
                  << "; final peak=" << maximum.peak[17] << '\n';
        testDecayStress(rate, 1.0f, 1.0f, true);
    }
}

void testEngine()
{
    for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        for (int knot = 0; knot < 5; ++knot)
            testImpulse(rate, knot);
        testVoiceTransitions(rate);
        testMixAndAutomation(rate);
        const auto small = render(rate, 0.5f, 0.35f, 64);
        const auto large = render(rate, 0.5f, 0.35f, 512);
        for (size_t channel = 0; channel < small.size(); ++channel)
            for (size_t i = 0; i < small[channel].size(); ++i)
                near(small[channel][i], large[channel][i], "block size independence");
        std::cout << rate << " Hz: all MEMORY, stereo, wrap, smoothing and MIX checks passed.\n";
    }
}
}

int main()
{
    try
    {
        testMacro();
        testFreeze();
        testReverse();
        testBuffer();
        testEngine();
        testDecay();
        testDrift();
        testBlur();
        std::cout << "All GHOST DSP tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
