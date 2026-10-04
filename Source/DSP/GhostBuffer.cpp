#include "GhostBuffer.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

void GhostBuffer::prepare(double sampleRate, double maxSeconds, int numChannels)
{
    const auto requestedSamples = sampleRate * maxSeconds;
    if (!std::isfinite(sampleRate) || !std::isfinite(maxSeconds)
        || sampleRate <= 0.0 || maxSeconds <= 0.0 || numChannels <= 0
        || !std::isfinite(requestedSamples)
        || requestedSamples > static_cast<double>(std::numeric_limits<int>::max() - 1))
        throw std::invalid_argument("Invalid GhostBuffer preparation dimensions");

    // Include the current sample PLUS the entire requested history. ceil()
    // also reserves the older neighbour needed for a fractional maximum delay.
    // Example: a 2.5-sample maximum needs ages 0, 1, 2 and 3 (four slots).
    const auto newCapacity = static_cast<int>(std::ceil(requestedSamples)) + 1;
    std::vector<std::vector<float>> newChannels(
        static_cast<size_t>(numChannels), std::vector<float>(static_cast<size_t>(newCapacity), 0.0f));
    std::vector<int> newPositions(static_cast<size_t>(numChannels), 0);
    std::vector<int> newValidSamples(static_cast<size_t>(numChannels), 0);
    channels.swap(newChannels);
    nextWritePositions.swap(newPositions);
    validSamples.swap(newValidSamples);
    seedFrames.assign(static_cast<size_t>(numChannels), 0);
    seed = nullptr;
    capturing = false;
    capacity = newCapacity;
    maximumDelaySamples = requestedSamples;
}

void GhostBuffer::reset() noexcept
{
    seed = nullptr;
    capturing = false;
    std::fill(seedFrames.begin(), seedFrames.end(), 0);
    std::fill(captureCounts.begin(), captureCounts.end(), 0);
    for (auto& channel : channels)
        std::fill(channel.begin(), channel.end(), 0.0f);
    std::fill(nextWritePositions.begin(), nextWritePositions.end(), 0);
    std::fill(validSamples.begin(), validSamples.end(), 0);
}

void GhostBuffer::writeSample(int channel, float sample) noexcept
{
    if (channel < 0 || static_cast<size_t>(channel) >= channels.size() || capacity == 0)
        return;

    const auto index = static_cast<size_t>(channel);
    auto& nextWrite = nextWritePositions[index];
    const auto slot = static_cast<size_t>(nextWrite);
    // Save the pre-capture value once, immediately before its first overwrite.
    // Unchanged slots remain readable in the live ring. At most one float and
    // one tag per channel are copied per frame, independent of history length.
    if (capturing && captureTags[index][slot] != generation)
    {
        captured[index][slot] = channels[index][slot];
        captureTags[index][slot] = generation;
    }
    channels[index][slot] = sample;
    if (seed != nullptr && seedFrames[index] < capacity)
        ++seedFrames[index];
    if (validSamples[index] < capacity)
        ++validSamples[index];
    // Overwrite the oldest slot when the cursor circles back to it.
    if (++nextWrite == capacity)
        nextWrite = 0;
}

int GhostBuffer::getValidSamples(int channel) const noexcept
{
    if (channel < 0 || static_cast<size_t>(channel) >= validSamples.size()) return 0;
    const auto index = static_cast<size_t>(channel);
    return seed == nullptr ? validSamples[index]
        : std::min(capacity, seedFrames[index] + seed->getCapturedSamples(channel));
}

bool GhostBuffer::isDelayRecorded(int channel, double delaySamples) const noexcept
{
    // A fractional read needs both neighbouring ages to have been recorded.
    // Age zero is the just-written sample, hence the strict < count comparison.
    return std::isfinite(delaySamples) && delaySamples >= 0.0
        && delaySamples <= maximumDelaySamples
        && std::ceil(delaySamples) < static_cast<double>(getValidSamples(channel));
}

float GhostBuffer::readDelayedSample(int channel, double delaySamples) const noexcept
{
    if (channel < 0 || static_cast<size_t>(channel) >= channels.size()
        || capacity == 0 || !std::isfinite(delaySamples))
        return 0.0f;

    delaySamples = std::clamp(delaySamples, 0.0, maximumDelaySamples);
    const auto index = static_cast<size_t>(channel);
    if (seed != nullptr)
    {
        // Integer ages select either a newly written working sample or the
        // immutable initial capture. Interpolate across that boundary as well.
        const auto a = static_cast<int>(std::floor(delaySamples));
        const auto fraction = static_cast<float>(delaySamples - a);
        const auto readAge = [&](int age) noexcept
        {
            if (age >= getValidSamples(channel)) return 0.0f;
            if (age >= seedFrames[index])
                return seed->readCapturedSample(channel, age - seedFrames[index]);
            auto slot = nextWritePositions[index] - 1 - age;
            if (slot < 0) slot += capacity;
            return channels[index][static_cast<size_t>(slot)];
        };
        const auto firstValue = readAge(a);
        return firstValue + fraction * (readAge(a + 1) - firstValue);
    }

    // writeSample advanced the cursor, so nextWrite - 1 is the newest sample.
    // Subtracting delay walks backwards through time. Add capacity if negative
    // to map that position back into [0, capacity). One addition is sufficient
    // because delay is clamped to at most capacity - 1.
    auto readPosition = static_cast<double>(nextWritePositions[index] - 1) - delaySamples;
    if (readPosition < 0.0)
        readPosition += static_cast<double>(capacity);
    // A sub-epsilon delay just below slot zero can round up to capacity when
    // added above. Treat that as slot zero, never as an out-of-bounds index.
    if (readPosition >= static_cast<double>(capacity))
        readPosition = 0.0;

    const auto first = static_cast<int>(std::floor(readPosition));
    const auto second = (first + 1 == capacity) ? 0 : first + 1;
    const auto fraction = static_cast<float>(readPosition - static_cast<double>(first));
    const auto& samples = channels[index];
    const auto a = samples[static_cast<size_t>(first)];
    const auto b = samples[static_cast<size_t>(second)];
    // Linear interpolation: fraction 0 returns a; 0.5 returns their midpoint.
    // The second neighbour wraps too, so interpolation works across the seam.
    return a + fraction * (b - a);
}

void GhostBuffer::prepareCapture()
{
    captured.assign(channels.size(), std::vector<float>(static_cast<size_t>(capacity), 0.0f));
    captureTags.assign(channels.size(), std::vector<std::uint64_t>(static_cast<size_t>(capacity), 0));
    capturePositions.assign(channels.size(), 0);
    captureCounts.assign(channels.size(), 0);
    generation = 0;
    capturing = false;
}

void GhostBuffer::capture() noexcept
{
    if (captured.size() != channels.size()) return;
    ++generation; // 64-bit epoch: over 5 billion years even at 100 captures/second.
    capturing = true;
    for (size_t channel = 0; channel < channels.size(); ++channel)
    {
        capturePositions[channel] = nextWritePositions[channel];
        captureCounts[channel] = validSamples[channel];
    }
}

int GhostBuffer::getCapturedSamples(int channel) const noexcept
{
    return capturing && channel >= 0 && static_cast<size_t>(channel) < captureCounts.size()
        ? captureCounts[static_cast<size_t>(channel)] : 0;
}

float GhostBuffer::readCapturedSample(int channel, double delay) const noexcept
{
    const auto count = getCapturedSamples(channel);
    if (!std::isfinite(delay) || delay < 0 || std::ceil(delay) >= count) return 0.0f;
    const auto index = static_cast<size_t>(channel);
    const auto age = static_cast<int>(std::floor(delay));
    const auto readAge = [&](int offset) noexcept
    {
        auto slot = capturePositions[index] - 1 - offset;
        if (slot < 0) slot += capacity;
        const auto position = static_cast<size_t>(slot);
        return captureTags[index][position] == generation
            ? captured[index][position] : channels[index][position];
    };
    const auto a = readAge(age);
    const auto fraction = static_cast<float>(delay - age);
    return fraction == 0.0f ? a : a + fraction * (readAge(age + 1) - a);
}

void GhostBuffer::seedFromCapture(const GhostBuffer& source) noexcept
{
    seed = &source;
    std::fill(nextWritePositions.begin(), nextWritePositions.end(), 0);
    std::fill(validSamples.begin(), validSamples.end(), 0);
    std::fill(seedFrames.begin(), seedFrames.end(), 0);
}
