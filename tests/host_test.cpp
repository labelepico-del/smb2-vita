// Host-side smoke test: loads the built SMB2 ROM, runs a few hundred frames,
// checks that audio is produced, and dumps screenshots. Not part of the VPK.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "emulator.h"

namespace {

void put16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
}

void put32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

bool writeBmp(const char* path, const std::uint8_t* rgba, int width, int height) {
    const int rowSize = (width * 3 + 3) & ~3;
    const std::uint32_t pixelBytes = static_cast<std::uint32_t>(rowSize * height);

    std::vector<std::uint8_t> file;
    file.reserve(54 + pixelBytes);

    file.push_back('B');
    file.push_back('M');
    put32(file, 54 + pixelBytes);
    put16(file, 0);
    put16(file, 0);
    put32(file, 54);
    put32(file, 40);
    put32(file, static_cast<std::uint32_t>(width));
    put32(file, static_cast<std::uint32_t>(height));
    put16(file, 1);
    put16(file, 24);
    put32(file, 0);
    put32(file, pixelBytes);
    put32(file, 2835);
    put32(file, 2835);
    put32(file, 0);
    put32(file, 0);

    for (int y = height - 1; y >= 0; --y) {
        const std::uint8_t* row = rgba + static_cast<std::size_t>(y) * width * 4;
        for (int x = 0; x < width; ++x) {
            file.push_back(row[x * 4 + 2]);
            file.push_back(row[x * 4 + 1]);
            file.push_back(row[x * 4 + 0]);
        }
        while (file.size() % 4 != 0) {
            file.push_back(0);
        }
    }

    FILE* f = std::fopen(path, "wb");
    if (!f) {
        return false;
    }
    const bool ok = std::fwrite(file.data(), 1, file.size(), f) == file.size();
    std::fclose(f);
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s rom.nes [output-prefix]\n", argv[0]);
        return 2;
    }
    const char* prefix = argc > 2 ? argv[2] : "frame";

    FILE* f = std::fopen(argv[1], "rb");
    if (!f) {
        std::perror(argv[1]);
        return 1;
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<std::uint8_t> rom(static_cast<std::size_t>(size));
    if (std::fread(rom.data(), 1, rom.size(), f) != rom.size()) {
        std::fclose(f);
        return 1;
    }
    std::fclose(f);

    Emulator emulator;
    if (!emulator.load(rom.data(), rom.size())) {
        std::fprintf(stderr, "load failed: %s\n", emulator.error());
        return 1;
    }
    std::printf("ROM loaded (%ld bytes), channels=%d\n", size, emulator.channels());

    std::vector<std::uint8_t> rgba(Emulator::width() * Emulator::height() * 4);
    long totalSamples = 0;
    double magnitude = 0.0;
    int nonSilentFrames = 0;

    for (int frame = 0; frame < 600; ++frame) {
        // Press Start for a few frames around 2 seconds, then A at 5 seconds,
        // just to exercise input handling.
        std::uint8_t buttons = 0;
        if (frame >= 120 && frame < 126) {
            buttons |= 0x08;
        }
        if (frame >= 300 && frame < 306) {
            buttons |= 0x01;
        }

        emulator.setButtons(buttons);
        emulator.runFrame();
        emulator.renderRgba(rgba.data(), Emulator::width() * 4);

        std::int16_t samples[4096];
        const long count = emulator.readSamples(samples, 4096);
        totalSamples += count;
        double frameMagnitude = 0.0;
        for (long i = 0; i < count; ++i) {
            frameMagnitude += std::abs(static_cast<int>(samples[i]));
        }
        magnitude += frameMagnitude;
        if (frameMagnitude > 0.0) {
            ++nonSilentFrames;
        }

        if (frame == 0 || frame == 60 || frame == 180 || frame == 360 || frame == 599) {
            char path[512];
            std::snprintf(path, sizeof(path), "%s-%04d.bmp", prefix, frame);
            if (!writeBmp(path, rgba.data(), Emulator::width(), Emulator::height())) {
                std::fprintf(stderr, "could not write %s\n", path);
                return 1;
            }
            std::printf("wrote %s\n", path);
        }
    }

    std::printf("audio: %ld samples over 600 frames (%.1f/frame), non-silent frames=%d, "
                "mean |sample|=%.2f\n",
                totalSamples, totalSamples / 600.0, nonSilentFrames,
                totalSamples > 0 ? magnitude / totalSamples : 0.0);
    if (emulator.error()) {
        std::fprintf(stderr, "emulator error: %s\n", emulator.error());
        return 1;
    }
    return 0;
}
