#pragma once

#include <cstddef>
#include <cstdint>

// Thin wrapper around the QuickNES core. Everything QuickNES-specific stays in
// emulator.cpp so the frontend only deals with plain buffers.
class Emulator {
public:
    Emulator();
    ~Emulator();

    Emulator(const Emulator&) = delete;
    Emulator& operator=(const Emulator&) = delete;

    // Loads an iNES image. Returns false on error; see error().
    bool load(const void* data, std::size_t size);
    const char* error() const;

    void reset();

    // NES joypad bits: A=0x01, B=0x02, Select=0x04, Start=0x08,
    // Up=0x10, Down=0x20, Left=0x40, Right=0x80.
    void setButtons(std::uint8_t buttons);

    // Runs exactly one video frame.
    void runFrame();

    // Copies the current frame as RGBA8 (R,G,B,A byte order) into dst.
    // stride is in bytes and must be >= width() * 4.
    void renderRgba(std::uint8_t* dst, int stride) const;

    // Reads the audio produced by the last frame. Returns the number of
    // samples written. channels() tells whether they are mono or stereo.
    long readSamples(std::int16_t* out, long capacity);
    int channels() const;

    static int width() { return 256; }
    static int height() { return 240; }

private:
    struct Impl;
    Impl* impl_;
};
