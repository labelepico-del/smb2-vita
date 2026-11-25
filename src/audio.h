#pragma once

#include <atomic>
#include <cstdint>

#include <psp2/types.h>

// Decouples QuickNES audio generation (one mono buffer per emulated frame)
// from the Vita audio port (fixed-size stereo grains). A small producer /
// consumer ring is fed by the emulation thread and drained by a dedicated
// audio thread, so sceAudioOutOutput() blocking never stalls emulation.
class AudioOutput {
public:
    AudioOutput();
    ~AudioOutput();

    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    bool start();
    void stop();

    // samples: mono or interleaved stereo int16. count is the number of
    // samples returned by QuickNES (mono) or total int16 values (stereo).
    void push(const std::int16_t* samples, long count, int channels);

private:
    static int threadEntry(SceSize args, void* argp);
    int threadLoop();

    SceUID thread_ = -1;
    int port_ = -1;
    std::atomic<bool> running_{false};

    static constexpr unsigned kRingFrames = 8192; // power of two
    static constexpr unsigned kRingMask = kRingFrames - 1;

    std::int16_t ring_[kRingFrames * 2] = {};
    std::atomic<unsigned> read_{0};
    std::atomic<unsigned> write_{0};
};
