#pragma once
#include "GhostBuffer.h"

// Immutable capture + a causally advancing working history. No audio copying
// at activation: GhostBuffer preserves captured slots before live overwrites.
class GhostFreeze
{
public:
    void prepare(double rate, int channels);
    void reset();
    void capture(GhostBuffer& live) noexcept;
    void writeFrame(float feedbackLeft, float feedbackRight) noexcept;
    const GhostBuffer& history() const noexcept { return working; }
    int capturedSamples() const noexcept { return length; }
private:
    float sourceSample(int channel) const noexcept;
    GhostBuffer working;
    const GhostBuffer* source = nullptr;
    int startupFrame = 0, startupLength = 1;
    int length = 0, position = 0, overlap = 0, maximumOverlap = 0;
};
