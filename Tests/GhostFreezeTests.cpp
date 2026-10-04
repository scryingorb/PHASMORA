#include "DSP/GhostEngine.h"
#include "DSP/GhostFreeze.h"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>

namespace
{
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void close(float a, float b, const char* why, float tolerance = 1.0e-6f)
{ check(std::isfinite(a) && std::abs(a - b) <= tolerance, why); }

void captureTest(double rate)
{
    GhostBuffer live, working;
    live.prepare(rate, 10.0, 2);
    live.prepareCapture();
    working.prepare(rate, 10.0, 2);
    const auto frames = static_cast<int>(rate * 2.0);
    for (int i = 0; i < frames; ++i)
    {
        live.writeSample(0, static_cast<float>(i) / frames);
        live.writeSample(1, -static_cast<float>(i) / frames);
    }
    live.capture();
    working.seedFromCapture(live);
    check(live.getCapturedSamples(0) == frames, "capture count must be actual two seconds");
    check(!working.isDelayRecorded(0, frames), "seed cannot invent full history");
    // Walk backward through an asymmetric ramp, then overwrite every live slot.
    for (int pass = 0; pass < 2; ++pass)
    {
        for (double delay = 0.0; delay < frames - 1; delay += 137.25)
        {
            const auto expected = static_cast<float>((frames - 1 - delay) / frames);
            close(live.readCapturedSample(0, delay), expected, "capture fractional ordering");
            close(live.readCapturedSample(1, delay), -expected, "capture stereo ordering");
            close(working.readDelayedSample(0, delay), expected, "lazy seeded history ordering");
        }
        for (int i = 0; i < static_cast<int>(rate * 11); ++i)
        { live.writeSample(0, -0.75f); live.writeSample(1, 0.125f); }
    }
    close(live.readDelayedSample(0, 100.5), -0.75f, "live continues independently");
    // Seed/replay splice must remain continuous even when the two ends of a
    // capture differ. Real-time writes may not jump back by the overlap length.
    GhostFreeze frozen;
    frozen.prepare(rate, 2);
    for (int i = 0; i < frames; ++i)
    { live.writeSample(0, static_cast<float>(i) / frames); live.writeSample(1, 0); }
    frozen.capture(live);
    auto previous = live.readCapturedSample(0, 0);
    for (int i = 0; i < static_cast<int>(rate * 0.25); ++i)
    {
        live.writeSample(0, 0.6f); live.writeSample(1, -0.6f);
        frozen.writeFrame(0, 0);
        const auto sample = frozen.history().readDelayedSample(0, 0);
        check(std::abs(sample - previous) < 0.001f, "capture/replay excitation seam bounded");
        previous = sample;
    }
    live.endCapture();
    live.capture();
    close(live.readCapturedSample(0, 0.5), 0.6f, "immediate recapture includes new live data");
    live.reset();
    working.reset();
    check(live.getCapturedSamples(0) == 0 && working.getValidSamples(0) == 0, "reset clears capture and seed");
}

std::vector<float> renderFreeze(double rate, int blockSize, bool unrelated, float mix,
                               bool reverse, float decay, bool automate = false, int side = 0)
{
    GhostEngine engine;
    engine.setMemory(0.0f); engine.setDecay(decay); engine.setDrift(1.0f);
    engine.setBlur(decay > 0 ? 1.0f : 0.0f); engine.setMix(mix); engine.setReverse(reverse);
    engine.prepare(rate, blockSize, 2);
    const auto total = static_cast<int>(rate * 12);
    std::vector<float> result(static_cast<size_t>(2 * total));
    // Event boundaries use exact sample positions, independent of host blocks.
    const std::array<int, 7> events { static_cast<int>(rate * 3), static_cast<int>(rate * 5),
        static_cast<int>(rate * 6), static_cast<int>(rate * 7), static_cast<int>(rate * 8),
        static_cast<int>(rate * 9), total };
    size_t event = 0;
    for (int frame = 0; frame < total;)
    {
        if (frame == events[event])
        {
            if (event == 0) engine.setFreeze(true);
            if (automate)
            {
                if (event == 1) engine.setMemory(1.0f);
                if (event == 2) engine.setReverse(!reverse);
                if (event == 3) engine.setMemory(0.5f);
                if (event == 4) engine.setFreeze(false);
                if (event == 5) engine.setFreeze(true);
            }
            ++event;
        }
        const auto count = std::min(blockSize, events[event] - frame);
        juce::AudioBuffer<float> buffer(2, count);
        for (int i = 0; i < count; ++i)
        {
            const auto t = static_cast<double>(frame + i) / rate;
            const auto a = static_cast<float>(0.25 * std::sin(2 * 3.14159265358979323846 * 5 * t)
                                            + 0.10 * std::sin(2 * 3.14159265358979323846 * 1.3 * t));
            const auto b = static_cast<float>(0.4 * std::sin(2 * 3.14159265358979323846 * 997 * t));
            const auto input = t < 3 ? a : (unrelated ? b : 0.0f);
            buffer.setSample(0, i, side == 2 ? 0.0f : input);
            buffer.setSample(1, i, side == 1 ? 0.0f : -0.5f * input);
        }
        engine.processBlock(buffer);
        for (int i = 0; i < count; ++i)
            for (int channel = 0; channel < 2; ++channel)
            {
                const auto sample = buffer.getSample(channel, i);
                check(std::isfinite(sample) && std::abs(sample) < 2.0f, "freeze render finite and bounded");
                result[static_cast<size_t>(2 * (frame + i) + channel)] = sample;
            }
        frame += count;
    }
    return result;
}

void startupTest(double rate)
{
    for (const bool reverse : { false, true })
    {
        GhostEngine wet, dry;
        for (auto* engine : { &wet, &dry })
        {
            engine->setFreeze(true); engine->setReverse(reverse); engine->setDecay(1);
            engine->setDrift(1); engine->setBlur(1); engine->prepare(rate, 64, 2);
        }
        wet.setMix(1); dry.setMix(0);
        for (int pass = 0; pass < 2; ++pass)
        {
            // Reset also settles mix; enabled empty freeze cannot capture later input.
            wet.reset(); dry.reset();
            for (int frame = 0; frame < static_cast<int>(rate * 2); frame += 64)
            {
                juce::AudioBuffer<float> a(2, 64), b(2, 64);
                for (int channel = 0; channel < 2; ++channel)
                    for (int i = 0; i < 64; ++i) { a.setSample(channel, i, 0.25f); b.setSample(channel, i, 0.25f); }
                wet.processBlock(a); dry.processBlock(b);
                for (int i = 0; i < 64; ++i)
                { close(a.getSample(0, i), 0, "empty freeze wet silence"); close(b.getSample(0, i), 0.25f, "empty freeze live dry"); }
            }
        }
    }
}

void rapidAndShortTest(double rate)
{
    GhostEngine engine;
    engine.setMemory(1); engine.setDecay(1); engine.setDrift(1); engine.setBlur(1); engine.setMix(1);
    engine.prepare(rate, 32, 2);
    juce::AudioBuffer<float> first(2, 1);
    first.setSample(0, 0, 0.1f); first.setSample(1, 0, -0.1f);
    engine.processBlock(first);
    engine.setFreeze(true); // Only ONE valid frame; reverse must wait safely.
    juce::AudioBuffer<float> block(2, 32);
    for (int frame = 0; frame < static_cast<int>(rate * 5); frame += 32)
    {
        if (frame > rate && frame < rate * 2)
        {
            engine.setFreeze((frame / 32) % 3 != 0);
            engine.setReverse((frame / 32) % 5 != 0);
        }
        else if (frame >= rate * 2) engine.setFreeze(true);
        for (int i = 0; i < 32; ++i)
        {
            const auto value = static_cast<float>(0.1 * std::sin((frame + i) / rate));
            block.setSample(0, i, value); block.setSample(1, i, -value);
        }
        engine.processBlock(block);
        for (int i = 0; i < 32; ++i)
            check(std::isfinite(block.getSample(0, i)) && std::abs(block.getSample(0, i)) < 1,
                  "one-frame capture and rapid recapture remain safe");
    }
}

void longTest(double rate)
{
    GhostEngine engine;
    engine.setGhost(1.0f);
    engine.setMemory(1); engine.setDecay(1); engine.setDrift(1); engine.setBlur(1);
    engine.setReverse(true); engine.setMix(1); engine.prepare(rate, 512, 2);
    const auto captureFrame = static_cast<int>(rate * 12);
    const auto total = static_cast<int>(rate * 192); // 12 s fill + full 180 s frozen.
    double early = 0, late = 0;
    float peak = 0;
    for (int frame = 0; frame < total;)
    {
        if (frame == captureFrame) engine.setFreeze(true);
        const auto boundary = frame < captureFrame ? captureFrame : total;
        const auto count = std::min(512, boundary - frame);
        juce::AudioBuffer<float> buffer(2, count);
        for (int i = 0; i < count; ++i)
        {
            const auto t = static_cast<double>(frame + i) / rate;
            const auto source = static_cast<float>(0.8 * std::sin(2 * 3.14159265358979323846 * 43 * t)
                                                  + 0.15 * std::sin(2 * 3.14159265358979323846 * 131 * t));
            buffer.setSample(0, i, frame < captureFrame ? source : 0.0f);
            buffer.setSample(1, i, frame < captureFrame ? -0.7f * source : 0.0f);
        }
        engine.processBlock(buffer);
        for (int i = 0; i < count; ++i)
            for (int channel = 0; channel < 2; ++channel)
            {
                const auto sample = buffer.getSample(channel, i);
                check(std::isfinite(sample) && std::abs(sample) < 2.0f, "freeze stress finite and bounded");
                if (frame + i >= captureFrame) peak = std::max(peak, std::abs(sample));
                const auto seconds = static_cast<double>(frame + i - captureFrame) / rate;
                if (seconds >= 50 && seconds < 60) early += static_cast<double>(sample) * sample;
                if (seconds >= 170 && seconds < 180) late += static_cast<double>(sample) * sample;
            }
        frame += count;
    }
    early /= rate * 20; late /= rate * 20;
    // Golden measurements retained from the verified pre-macro FREEZE build.
    const auto expectedPeak = rate == 44100 ? 0.697559f : rate == 48000 ? 0.697102f
                            : rate == 88200 ? 0.695974f : 0.695503f;
    close(peak, expectedPeak, "GHOST 100 preserves pre-macro all-max FREEZE peak", 3.0e-6f);
    check(early > 1.0e-8 && late > 1.0e-8 && late / early < 4, "freeze retains stable bounded texture");
    std::cout << rate << " Hz FREEZE 180s peak=" << peak << " (" << 20 * std::log10(peak)
              << " dBFS); energy 50-60s=" << early << ", 170-180s=" << late
              << "; ratio=" << late / early << std::endl;
}
}

void testFreeze()
{
    for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        captureTest(rate); startupTest(rate); rapidAndShortTest(rate);
        for (const bool reverse : { false, true })
            for (const float decay : { 0.0f, 0.5f, 1.0f })
            {
                const auto silent = renderFreeze(rate, 256, false, 1, reverse, decay);
                const auto newInput = renderFreeze(rate, 256, true, 1, reverse, decay);
                double energy = 0;
                float difference = 0;
                for (size_t i = static_cast<size_t>(rate * 4 * 2); i < silent.size(); ++i)
                {
                    difference = std::max(difference, std::abs(silent[i] - newInput[i]));
                    energy += static_cast<double>(silent[i]) * silent[i];
                }
                std::cout << rate << " Hz FREEZE isolation reverse=" << reverse << " decay=" << decay
                          << " difference=" << difference << std::endl;
                check(difference < 1.0e-6f, "new live signal must not contaminate frozen wet memory");
                check(energy > 0.001, "captured material persists even at DECAY zero");
            }
        const auto dry = renderFreeze(rate, 64, true, 0, true, 1);
        for (int frame = static_cast<int>(rate * 4); frame < static_cast<int>(rate * 12); ++frame)
        {
            const auto expected = static_cast<float>(0.4 * std::sin(2 * 3.14159265358979323846 * 997 * frame / rate));
            close(dry[static_cast<size_t>(2 * frame)], expected, "new live B remains dry");
        }
        const auto reference = renderFreeze(rate, 32, false, 1, false, 1, true);
        float jump = 0;
        for (size_t i = static_cast<size_t>(rate * 2 * 2); i < reference.size(); ++i)
            jump = std::max(jump, std::abs(reference[i] - reference[i - 2]));
        check(jump < 0.003f, "freeze transitions bounded on smooth test signal");
        for (const int block : { 64, 256, 512, 1024 })
        {
            const auto output = renderFreeze(rate, block, false, 1, false, 1, true);
            for (size_t i = 0; i < output.size(); ++i) close(output[i], reference[i], "freeze block independence", 0.0f);
        }
        for (const int side : { 1, 2 })
        {
            const auto output = renderFreeze(rate, 256, false, 1, true, 1, true, side);
            const auto silentChannel = side == 1 ? 1 : 0;
            for (size_t i = static_cast<size_t>(silentChannel); i < output.size(); i += 2)
                close(output[i], 0, "freeze must not crossfeed");
        }
        std::cout << rate << " Hz FREEZE capture/startup/reset/automation/stereo/block tests passed; max step=" << jump << std::endl;
        longTest(rate);
    }
}
