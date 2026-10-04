#pragma once
#include "GhostBuffer.h"
#include <cstdint>

struct GhostReverseSettings
{
    double memoryRatio = 0.20;
    double minimumSeconds = 0.15;
    double maximumSeconds = 0.50;
};

// Two fixed-state stereo heads; all audio remains in the shared GhostBuffer.
// Audio-thread only. Region creation never allocates or copies audio.
class GhostReverse
{
public:
    void prepare(double sampleRate, double maximumDriftSamples, GhostReverseSettings settings) noexcept;
    void reset() noexcept;
    void setDelaySamples(double delay) noexcept;
    void processSample(const GhostBuffer& history, double driftOffset, float& left, float& right) noexcept;

    // Read-only audio-thread diagnostics for DSP verification.
    bool isCrossfading() const noexcept { return fading; }
    bool hasRegion() const noexcept { return current.active || incoming.active; }
    double getReadDelaySamples() const noexcept { return lastRead; }
    double getIncomingReadDelaySamples() const noexcept { return lastIncomingRead; }
    std::uint64_t getRejectedReads() const noexcept { return rejectedReads; }

private:
    struct Region
    {
        double startDelay = 0.0;
        double requestedBase = 0.0;
        int length = 0;
        int age = 0;
        bool active = false;
    };
    GhostReverseSettings configuration;
    Region current, incoming;
    double rate = 0.0;
    double driftGuard = 0.0;
    double requestedDelay = 0.0;
    int crossfadeLength = 1;
    int crossfadePosition = 0;
    bool fading = false;
    double lastRead = -1.0, lastIncomingRead = -1.0;
    std::uint64_t rejectedReads = 0;
    bool makeRegion(const GhostBuffer& history, double baseDelay, Region& result) const noexcept;
    void read(const GhostBuffer& history, const Region& region, double offset,
              float& left, float& right, double& readDelay) noexcept;
};
