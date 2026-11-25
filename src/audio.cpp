#include "audio.h"

#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>

namespace {
constexpr int kPortFrames = 512; // must be a multiple of SCE_AUDIO_MIN_LEN
} // namespace

AudioOutput::AudioOutput() = default;

AudioOutput::~AudioOutput() {
    stop();
}

bool AudioOutput::start() {
    if (port_ >= 0) {
        return true;
    }

    port_ = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, kPortFrames,
                                SCE_AUDIO_OUT_SAMPLE_RATE_48000,
                                SCE_AUDIO_OUT_MODE_STEREO);
    if (port_ < 0) {
        port_ = -1;
        return false;
    }

    int volumes[2] = {SCE_AUDIO_OUT_MAX_VOL, SCE_AUDIO_OUT_MAX_VOL};
    sceAudioOutSetVolume(port_, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH,
                         volumes);

    read_.store(0, std::memory_order_relaxed);
    write_.store(0, std::memory_order_relaxed);
    running_.store(true, std::memory_order_release);

    thread_ = sceKernelCreateThread("smb2_audio", &AudioOutput::threadEntry, 0x10000100,
                                    0x4000, 0, 0, nullptr);
    if (thread_ < 0) {
        running_.store(false, std::memory_order_release);
        sceAudioOutReleasePort(port_);
        port_ = -1;
        return false;
    }

    AudioOutput* self = this;
    if (sceKernelStartThread(thread_, sizeof(self), &self) < 0) {
        running_.store(false, std::memory_order_release);
        sceKernelDeleteThread(thread_);
        thread_ = -1;
        sceAudioOutReleasePort(port_);
        port_ = -1;
        return false;
    }

    return true;
}

void AudioOutput::stop() {
    if (port_ < 0) {
        return;
    }

    running_.store(false, std::memory_order_release);

    if (thread_ >= 0) {
        SceUInt timeout = 250000; // microseconds; port call returns in ~11 ms
        sceKernelWaitThreadEnd(thread_, nullptr, &timeout);
        sceKernelDeleteThread(thread_);
        thread_ = -1;
    }

    sceAudioOutReleasePort(port_);
    port_ = -1;
}

void AudioOutput::push(const std::int16_t* samples, long count, int channels) {
    if (!samples || count <= 0 || port_ < 0) {
        return;
    }

    unsigned frames = (channels >= 2) ? static_cast<unsigned>(count / 2)
                                      : static_cast<unsigned>(count);

    const unsigned w = write_.load(std::memory_order_relaxed);
    const unsigned r = read_.load(std::memory_order_acquire);
    const unsigned space = kRingFrames - (w - r);
    if (frames > space) {
        // Audio is the clock master; drop new samples rather than overwrite
        // audio that has not been played yet.
        frames = space;
    }

    for (unsigned i = 0; i < frames; ++i) {
        std::int16_t left;
        std::int16_t right;
        if (channels >= 2) {
            left = samples[i * 2];
            right = samples[i * 2 + 1];
        } else {
            left = right = samples[i];
        }
        const unsigned idx = (w + i) & kRingMask;
        ring_[idx * 2] = left;
        ring_[idx * 2 + 1] = right;
    }

    write_.store(w + frames, std::memory_order_release);
}

int AudioOutput::threadEntry(SceSize, void* argp) {
    AudioOutput* self = *static_cast<AudioOutput**>(argp);
    return self->threadLoop();
}

int AudioOutput::threadLoop() {
    std::int16_t grain[kPortFrames * 2];

    while (running_.load(std::memory_order_acquire)) {
        const unsigned r = read_.load(std::memory_order_relaxed);
        const unsigned w = write_.load(std::memory_order_acquire);
        if (w - r < static_cast<unsigned>(kPortFrames)) {
            sceKernelDelayThread(1000);
            continue;
        }

        for (int i = 0; i < kPortFrames; ++i) {
            const unsigned idx = (r + static_cast<unsigned>(i)) & kRingMask;
            grain[i * 2] = ring_[idx * 2];
            grain[i * 2 + 1] = ring_[idx * 2 + 1];
        }
        read_.store(r + kPortFrames, std::memory_order_release);

        // Blocks until the grain has been consumed; this paces the consumer.
        sceAudioOutOutput(port_, grain);
    }

    return 0;
}
