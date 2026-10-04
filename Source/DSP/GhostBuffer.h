#pragma once
#include <vector>
#include <cstdint>

// Single-audio-thread history. prepare() allocates; all other operations reuse
// that storage. Do not call prepare/reset concurrently with reads or writes.
class GhostBuffer
{
public:
    void prepare(double sampleRate, double maxSeconds, int numChannels);
    void reset() noexcept;
    void writeSample(int channel, float sample) noexcept;

    // Read AFTER writing the current sample: delay 0 = that sample, delay 1 =
    // the previous sample. Write each channel once per frame to keep stereo
    // aligned. Independent cursors mean channel call order does not matter.
    // Unwritten history is silence. Finite delays are clamped to the prepared
    // range; invalid channels/non-finite delays return silence.
    float readDelayedSample(int channel, double delaySamples) const noexcept;
    // Optional copy-on-write capture storage is allocated only during prepare.
    void prepareCapture();
    void capture() noexcept;
    void endCapture() noexcept { capturing = false; }
    int getCapturedSamples(int channel) const noexcept;
    float readCapturedSample(int channel, double delay) const noexcept;
    // Constant-time logical clear/seed; old physical slots cannot become valid.
    void seedFromCapture(const GhostBuffer& source) noexcept;
    int getValidSamples(int channel) const noexcept;
    double getMaximumDelaySamples() const noexcept { return maximumDelaySamples; }
    bool isDelayRecorded(int channel, double delaySamples) const noexcept;

private:
    std::vector<std::vector<float>> channels;
    std::vector<int> nextWritePositions;
    std::vector<int> validSamples;
    std::vector<std::vector<float>> captured;
    std::vector<std::vector<std::uint64_t>> captureTags;
    std::vector<int> capturePositions, captureCounts;
    std::uint64_t generation = 0;
    bool capturing = false;
    const GhostBuffer* seed = nullptr;
    std::vector<int> seedFrames;
    int capacity = 0;
    double maximumDelaySamples = 0.0;
};
