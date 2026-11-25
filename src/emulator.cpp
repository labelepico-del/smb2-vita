#include "emulator.h"

#include <cstring>
#include <new>
#include <vector>

#include "Data_Reader.h"
#include "Nes_Emu.h"

struct Emulator::Impl {
    Nes_Emu emu;
    std::vector<std::uint8_t> video;
    const char* error = nullptr;
    std::uint8_t buttons = 0;
};

Emulator::Emulator()
    : impl_(new (std::nothrow) Impl()) {
}

Emulator::~Emulator() {
    delete impl_;
}

bool Emulator::load(const void* data, std::size_t size) {
    if (!impl_ || !data || size < 16) {
        return false;
    }

    // QuickNES renders 8-bit palette indices into a buffer that is slightly
    // wider and taller than the visible 256x240 image.
    impl_->video.assign(
        static_cast<std::size_t>(Nes_Emu::buffer_width) * impl_->emu.buffer_height(), 0);

    const char* err = impl_->emu.set_sample_rate(48000);
    if (err) {
        impl_->error = err;
        return false;
    }

    Mem_File_Reader reader(data, static_cast<long>(size));
    err = impl_->emu.load_ines(reader);
    if (err) {
        impl_->error = err;
        return false;
    }

    impl_->emu.set_pixels(impl_->video.data(), Nes_Emu::buffer_width);
    impl_->emu.set_sprite_mode(Nes_Emu::sprites_visible);
    impl_->emu.set_equalizer(Nes_Emu::nes_eq);
    return true;
}

const char* Emulator::error() const {
    return impl_ ? impl_->error : "out of memory";
}

void Emulator::reset() {
    if (impl_) {
        impl_->emu.reset(false, false);
    }
}

void Emulator::setButtons(std::uint8_t buttons) {
    if (impl_) {
        impl_->buttons = buttons;
    }
}

void Emulator::runFrame() {
    if (!impl_) {
        return;
    }
    const char* err = impl_->emu.emulate_frame(impl_->buttons, 0);
    if (err) {
        impl_->error = err;
    }
}

void Emulator::renderRgba(std::uint8_t* dst, int stride) const {
    if (!impl_ || !dst || stride < width() * 4) {
        return;
    }

    const Nes_Emu::frame_t& frame = impl_->emu.frame();

    // Build a 256-entry RGBA lookup table for this frame. QuickNES gives us
    // palette indices into an 8-bit framebuffer and a per-frame map into its
    // 512-entry (8 emphasis modes x 64 colours) RGB table.
    std::uint32_t lut[256];
    for (unsigned i = 0; i < 256; ++i) {
        const Nes_Emu::rgb_t& rgb = Nes_Emu::nes_colors[frame.palette[i] & 0x1FF];
        lut[i] = static_cast<std::uint32_t>(rgb.red) |
                 (static_cast<std::uint32_t>(rgb.green) << 8) |
                 (static_cast<std::uint32_t>(rgb.blue) << 16) |
                 0xFF000000u;
    }

    for (int y = 0; y < height(); ++y) {
        const std::uint8_t* src = frame.pixels + static_cast<long>(y) * frame.pitch;
        std::uint32_t* out = reinterpret_cast<std::uint32_t*>(dst + static_cast<std::size_t>(y) * stride);
        for (int x = 0; x < width(); ++x) {
            out[x] = lut[src[x]];
        }
    }
}

long Emulator::readSamples(std::int16_t* out, long capacity) {
    if (!impl_ || !out || capacity <= 0) {
        return 0;
    }
    return impl_->emu.read_samples(out, capacity);
}

int Emulator::channels() const {
    if (!impl_) {
        return 1;
    }
    return impl_->emu.frame().chan_count ? impl_->emu.frame().chan_count : 1;
}
