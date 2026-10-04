#include "GhostReverse.h"
#include <algorithm>
#include <cmath>

void GhostReverse::prepare(double sampleRate, double maximumDriftSamples, GhostReverseSettings settings) noexcept
{
    rate = sampleRate;
    configuration = settings;
    driftGuard = std::max(0.0, maximumDriftSamples);
    crossfadeLength = std::max(1, static_cast<int>(std::round(rate * 0.05)));
    reset();
}

void GhostReverse::reset() noexcept
{
    current = {};
    incoming = {};
    fading = false;
    crossfadePosition = 0;
    lastRead = lastIncomingRead = -1.0;
    rejectedReads = 0;
}

void GhostReverse::setDelaySamples(double delay) noexcept
{
    if (std::isfinite(delay))
        requestedDelay = std::max(0.0, delay);
}

bool GhostReverse::makeRegion(const GhostBuffer& history, double baseDelay, Region& result) const noexcept
{
    if (rate <= 0.0 || configuration.memoryRatio <= 0.0)
        return false;
    // Undo the voice ratio to obtain the existing MEMORY depth D in [0.5,10] s.
    // This gives short phrases near the bottom, long phrases near the top.
    const auto depthSeconds = baseDelay / (rate * configuration.memoryRatio);
    const auto u = std::clamp((depthSeconds - 0.5) / 9.5, 0.0, 1.0);
    const auto seconds = configuration.minimumSeconds
                       + u * (configuration.maximumSeconds - configuration.minimumSeconds);
    const auto length = std::max(2 * crossfadeLength + 1, static_cast<int>(std::round(rate * seconds)));

    // As write time advances +1 and source playback moves -1, the delay from
    // NOW grows +2 per frame. Reserve the complete 2*length traversal plus the
    // maximum possible DRIFT excursion, even if DRIFT is currently zero.
    // A deep/long region may need a newer anchor to avoid being overwritten.
    const auto lower = driftGuard + 2.0;
    const auto upper = history.getMaximumDelaySamples() - 2.0 * length - driftGuard - 2.0;
    if (upper < lower)
        return false;
    const auto start = std::clamp(baseDelay, lower, upper);

    // At selection time, the OLDEST source sample we may visit is start+length
    // samples ago (not 2*length: the extra length is future write-cursor motion).
    // Require that entire region, interpolation neighbours and DRIFT margin
    // have already been recorded. Otherwise wait silently/fall back to a region
    // at the previous MEMORY depth; never shorten by reading unwritten audio.
    const auto oldestSource = start + length + driftGuard + 2.0;
    if (!history.isDelayRecorded(0, oldestSource) || !history.isDelayRecorded(1, oldestSource))
        return false;
    result = { start, baseDelay, length, 0, true };
    return true;
}

void GhostReverse::read(const GhostBuffer& history, const Region& region, double offset,
                        float& left, float& right, double& readDelay) noexcept
{
    left = right = 0.0f;
    readDelay = -1.0;
    if (!region.active)
        return;
    readDelay = region.startDelay + 2.0 * region.age + offset;
    // Explicit checks prevent GhostBuffer's clamping/cleared storage from hiding
    // reverse errors. No block reversal and no separate left/right trajectories.
    if (region.age >= region.length || !history.isDelayRecorded(0, readDelay)
        || !history.isDelayRecorded(1, readDelay))
    {
        ++rejectedReads;
        return;
    }
    left = history.readDelayedSample(0, readDelay);
    right = history.readDelayedSample(1, readDelay);
}

void GhostReverse::processSample(const GhostBuffer& history, double driftOffset,
                                float& left, float& right) noexcept
{
    if (!fading && (!current.active || current.age >= current.length - crossfadeLength))
    {
        if (makeRegion(history, requestedDelay, incoming)
            || (current.active && makeRegion(history, current.requestedBase, incoming)))
        {
            fading = true;
            crossfadePosition = 0;
        }
    }
    const auto offset = std::isfinite(driftOffset) ? std::clamp(driftOffset, -driftGuard, driftGuard) : 0.0;
    read(history, current, offset, left, right, lastRead);
    lastIncomingRead = -1.0;
    if (fading)
    {
        float nextLeft = 0.0f, nextRight = 0.0f;
        read(history, incoming, offset, nextLeft, nextRight, lastIncomingRead);
        constexpr double pi = 3.14159265358979323846;
        const auto t = static_cast<double>(++crossfadePosition) / crossfadeLength;
        // Raised-cosine complementary weights: smooth slope at both ends, no
        // equal-power boost for correlated history. First region fades from zero.
        const auto weight = static_cast<float>(0.5 - 0.5 * std::cos(pi * t));
        left += weight * (nextLeft - left);
        right += weight * (nextRight - right);
        ++incoming.age;
    }
    if (current.active)
        ++current.age;
    if (fading && crossfadePosition == crossfadeLength)
    {
        current = incoming;
        incoming = {};
        fading = false;
    }
}
