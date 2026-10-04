#include "DSP/GhostEngine.h"
#include "DSP/GhostMacro.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#include <algorithm>

namespace
{
constexpr double pi = 3.14159265358979323846;
void check(bool condition, const char* why) { if (!condition) throw std::runtime_error(why); }
void near(double a, double b, const char* why, double tolerance = 1.0e-6)
{ check(std::isfinite(a) && std::abs(a - b) <= tolerance, why); }

void mappingTest(double rate)
{
    double previous = -1;
    for (double g : { 0.0, 0.25, 0.5, 0.75, 1.0 })
    {
        const auto scales = GhostMacro::map(g);
        near(scales.level, g * g, "macro level curve");
        near(scales.width, std::pow(g, 1.5), "macro width curve");
        near(scales.drift, g, "macro drift curve");
        near(scales.blur, std::pow(g, 1.5), "macro blur curve");
        near(scales.decay, g * g, "macro decay input curve");
        const auto feedback = 0.95 * scales.decay * (2 - scales.decay);
        check(feedback >= 0 && feedback <= 0.95 && feedback >= previous, "macro feedback safe monotonic curve");
        previous = feedback;
    }
    GhostMacro macro;
    macro.prepare(rate);
    macro.setAmount(0);
    const auto frames = static_cast<int>(rate * 0.15);
    for (int i = 0; i < frames; ++i)
    {
        const auto frame = macro.next();
        const auto expected = 1.0 - static_cast<double>(i + 1) / frames;
        near(frame.level, expected * expected, "150 ms gain ramp", 1.0e-10);
    }
    near(macro.next().level, 0, "macro settles exactly zero", 0);
    macro.setAmount(std::numeric_limits<float>::quiet_NaN());
    near(macro.targets().level, 0, "invalid macro ignored", 0);
    macro.setAmount(2); macro.reset(); near(macro.next().level, 1, "macro upper clamp", 0);

    GhostBuffer history;
    history.prepare(rate, 10, 2);
    for (int i = 0; i < static_cast<int>(rate * 0.2); ++i)
    { history.writeSample(0, 1); history.writeSample(1, 1); }
    for (const float pan : { -0.25f, 0.25f, -0.55f, 0.55f })
    {
        GhostVoice voice;
        voice.setDrift(0);
        voice.prepare(rate, 10, 0.05, -12, pan);
        double previousBalance = -1;
        for (const double g : { 0.0, 0.5, 1.0 })
        {
            voice.setStereoWidth(GhostMacro::map(g).width);
            float left = 0, right = 0;
            voice.processSample(history, left, right);
            const auto angle = (pan * std::pow(g, 1.5) + 1) * pi / 4;
            near(left, std::pow(10.0, -12.0 / 20) * std::cos(angle), "left macro pan law");
            near(right, std::pow(10.0, -12.0 / 20) * std::sin(angle), "right macro pan law");
            const auto balance = std::abs(static_cast<double>(left - right));
            check(balance >= previousBalance, "width intermediate between center and original pan");
            previousBalance = balance;
        }
        const auto phase = voice.getPhaseCycles();
        voice.setDrift(0); voice.setStereoWidth(0);
        near(voice.getPhaseCycles(), phase, "macro operations cannot reset LFO phase", 0);
    }
}

std::vector<float> render(double rate, int blockSize, float mix, int side = 0)
{
    GhostEngine engine;
    engine.setGhost(1); engine.setMemory(0); engine.setDecay(1); engine.setDrift(1);
    engine.setBlur(1); engine.setReverse(true); engine.setMix(mix);
    engine.prepare(rate, blockSize, 2);
    const std::array<double, 13> seconds { 3, 4, 7, 8, 9, 9.02, 9.04, 9.06, 10, 13, 15, 16, 18 };
    std::array<int, 13> events {};
    for (size_t i = 0; i < events.size(); ++i) events[i] = static_cast<int>(std::round(rate * seconds[i]));
    const auto total = events.back();
    std::vector<float> output(static_cast<size_t>(total * 2));
    size_t event = 0;
    for (int frame = 0; frame < total;)
    {
        if (frame == events[event])
        {
            if (event == 0) engine.setFreeze(true);
            if (event == 1 || event == 4 || event == 6 || event == 8) engine.setGhost(0);
            if (event == 2) engine.setGhost(0.5f);
            if (event == 3 || event == 5 || event == 7 || event == 9) engine.setGhost(1);
            if (event == 10) engine.setMemory(1);
            if (event == 11) engine.setFreeze(false);
            ++event;
        }
        const auto count = std::min(blockSize, events[event] - frame);
        juce::AudioBuffer<float> block(2, count);
        for (int i = 0; i < count; ++i)
        {
            const auto t = static_cast<double>(frame + i) / rate;
            const auto input = static_cast<float>(0.4 * std::sin(2 * pi * 5 * t) + 0.1 * std::sin(2 * pi * 1.3 * t));
            block.setSample(0, i, side == 2 ? 0 : input);
            block.setSample(1, i, side == 1 ? 0 : -0.5f * input);
        }
        engine.processBlock(block);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < count; ++i)
            {
                const auto value = block.getSample(channel, i);
                check(std::isfinite(value) && std::abs(value) < 2, "macro automation finite and bounded");
                output[static_cast<size_t>(2 * (frame + i) + channel)] = value;
            }
        frame += count;
    }
    return output;
}

void automationTest(double rate)
{
    const auto reference = render(rate, 32, 1);
    float step = 0, peak = 0;
    double silentEnergy = 0, restoredEnergy = 0;
    for (int frame = 1; frame < static_cast<int>(rate * 18); ++frame)
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto i = static_cast<size_t>(2 * frame + channel);
            peak = std::max(peak, std::abs(reference[i]));
            if (frame >= rate * 3) step = std::max(step, std::abs(reference[i] - reference[i - 2]));
            if ((frame >= rate * 5 && frame < rate * 7) || (frame >= rate * 11 && frame < rate * 13))
                silentEnergy += static_cast<double>(reference[i]) * reference[i];
            if (frame >= rate * 14 && frame < rate * 15)
                restoredEnergy += static_cast<double>(reference[i]) * reference[i];
        }
    check(silentEnergy == 0, "macro zero silences wet after settling");
    check(restoredEnergy / (rate * 2) > 1.0e-6, "frozen reverse capture survives several seconds at macro zero");
    check(step < 0.003f, "macro transitions bounded on smooth signal");
    for (const int block : { 64, 256, 512, 1024 })
    {
        const auto output = render(rate, block, 1);
        for (size_t i = 0; i < output.size(); ++i) near(output[i], reference[i], "macro block independence", 0);
    }
    const auto dry = render(rate, 256, 0);
    for (int frame = 0; frame < static_cast<int>(rate * 18); ++frame)
    {
        const auto t = static_cast<double>(frame) / rate;
        const auto expected = static_cast<float>(0.4 * std::sin(2 * pi * 5 * t) + 0.1 * std::sin(2 * pi * 1.3 * t));
        near(dry[static_cast<size_t>(2 * frame)], expected, "macro must preserve dry L exactly", 0);
        near(dry[static_cast<size_t>(2 * frame + 1)], -0.5f * expected, "macro must preserve dry R exactly", 0);
    }
    for (const int side : { 1, 2 })
    {
        const auto output = render(rate, 256, 1, side);
        for (size_t i = side == 1 ? 1u : 0u; i < output.size(); i += 2)
            near(output[i], 0, "macro does not crossfeed", 0);
    }
    std::cout << rate << " Hz MACRO mapping/width/dry/block/persistence passed; zero RMS=0; max step="
              << step << "; automation peak=" << peak << " (" << 20 * std::log10(peak) << " dBFS)" << std::endl;
}

void longAutomation(double rate)
{
    GhostEngine engine;
    engine.setGhost(1); engine.setMemory(1); engine.setDecay(1); engine.setDrift(1);
    engine.setBlur(1); engine.setReverse(true); engine.setMix(1); engine.prepare(rate, 512, 2);
    const auto second = static_cast<int>(rate);
    float peak = 0;
    double lateEnergy = 0;
    // Populate 12 s, then exercise 180 s of frozen maximum-feedback automation.
    for (int frame = 0; frame < second * 192;)
    {
        if (frame == second * 12) engine.setFreeze(true);
        const auto count = std::min(512, second - frame % second);
        if (frame >= second * 12 && frame % second == 0)
            engine.setGhost(((frame / second - 12) / 5) % 2 == 0 ? 0.0f : 1.0f);
        juce::AudioBuffer<float> block(2, count);
        for (int i = 0; i < count; ++i)
        {
            const auto t = static_cast<double>(frame + i) / rate;
            const auto source = frame < second * 12
                ? static_cast<float>(0.8 * std::sin(2 * pi * 43 * t) + 0.15 * std::sin(2 * pi * 131 * t)) : 0;
            block.setSample(0, i, source); block.setSample(1, i, -0.7f * source);
        }
        engine.processBlock(block);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < count; ++i)
            {
                const auto sample = block.getSample(channel, i);
                check(std::isfinite(sample) && std::abs(sample) < 2, "long macro automation bounded");
                peak = std::max(peak, std::abs(sample));
                if (frame + i >= second * 187) lateEnergy += static_cast<double>(sample) * sample;
            }
        frame += count;
    }
    check(lateEnergy / (rate * 10) > 1.0e-7, "capture survives repeated 180s macro muting");
    std::cout << rate << " Hz MACRO 180s frozen 0/100 automation peak=" << peak
              << " (" << 20 * std::log10(peak) << " dBFS); final audible-window mean square="
              << lateEnergy / (rate * 10) << std::endl;
}
}
void testMacro()
{
    for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    { mappingTest(rate); automationTest(rate); longAutomation(rate); }
}
