#include "GhostFreeze.h"
#include <algorithm>
#include <cmath>

void GhostFreeze::prepare(double rate, int channels)
{
    working.prepare(rate, 10.0, channels);
    startupLength = std::max(1, static_cast<int>(std::round(rate * 0.15)));
    startupFrame = 0;
    maximumOverlap = std::max(1, static_cast<int>(std::round(rate * 0.05)));
    source = nullptr;
    length = position = overlap = 0;
}
void GhostFreeze::reset()
{
    working.reset();
    startupFrame = 0;
    source = nullptr;
    length = position = overlap = 0;
}
void GhostFreeze::capture(GhostBuffer& live) noexcept
{
    live.capture();
    startupFrame = 0;
    source = &live;
    length = std::min(live.getCapturedSamples(0), live.getCapturedSamples(1));
    overlap = std::min(maximumOverlap, length / 4);
    // Start with the capture's tail, smoothly joining its beginning. Subsequent
    // cycles skip the already-overlapped beginning: period = length - overlap.
    position = length - overlap;
    if (position == length) position = 0;
    working.seedFromCapture(live);
}
float GhostFreeze::sourceSample(int channel) const noexcept
{
    if (source == nullptr || length == 0) return 0.0f;
    auto sample = source->readCapturedSample(channel, length - 1 - position);
    if (overlap != 0 && position >= length - overlap)
    {
        const auto offset = position - (length - overlap);
        const auto b = source->readCapturedSample(channel, length - 1 - offset);
        const auto t = static_cast<double>(offset + 1) / overlap;
        const auto weight = static_cast<float>(0.5 - 0.5 * std::cos(3.14159265358979323846 * t));
        sample += weight * (b - sample);
    }
    // The first appended sample must join the newest captured sample, not jump
    // back by the loop overlap. Fade that endpoint into replay over 150 ms.
    // This is a temporary excitation splice, never a frozen dry output.
    if (startupFrame < startupLength)
    {
        const auto t = static_cast<double>(startupFrame + 1) / startupLength;
        const auto weight = static_cast<float>(0.5 - 0.5 * std::cos(3.14159265358979323846 * t));
        const auto endpoint = source->readCapturedSample(channel, 0.0);
        sample = endpoint + weight * (sample - endpoint);
    }
    return sample;
}
void GhostFreeze::writeFrame(float feedbackLeft, float feedbackRight) noexcept
{
    // Empty captures stay truly empty, even when live input later arrives.
    if (length == 0) return;
    working.writeSample(0, sourceSample(0) + feedbackLeft);
    working.writeSample(1, sourceSample(1) + feedbackRight);
    if (startupFrame < startupLength) ++startupFrame;
    if (++position >= length) position = overlap;
}
