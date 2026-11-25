#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/power.h>
#include <vita2d.h>

#include <cstdint>

#include "audio.h"
#include "emulator.h"
#include "rom_data.h"

namespace {

// NES joypad bits understood by QuickNES.
enum NesButton : std::uint8_t {
    NES_A = 0x01,
    NES_B = 0x02,
    NES_SELECT = 0x04,
    NES_START = 0x08,
    NES_UP = 0x10,
    NES_DOWN = 0x20,
    NES_LEFT = 0x40,
    NES_RIGHT = 0x80,
};

constexpr int kScreenWidth = 960;
constexpr int kScreenHeight = 544;
constexpr int kTurboPeriod = 4; // 2 frames on, 2 frames off -> 15 Hz at 60 fps

// The NES output is 256x240 (4:3). On the Vita's 960x544 (16:9) screen we use
// a single uniform scale so pixels keep their aspect ratio, letterboxing the
// sides with black bars instead of stretching the image.
constexpr float kScale = static_cast<float>(kScreenHeight) / Emulator::height();
constexpr float kScaleX = kScale;
constexpr float kScaleY = kScale;
constexpr float kOffsetX = (kScreenWidth - Emulator::width() * kScaleX) * 0.5f;

std::uint8_t mapButtons(std::uint32_t buttons, bool& turboA, bool& turboB, int& turboPhase) {
    std::uint8_t nes = 0;

    if (buttons & SCE_CTRL_UP) {
        nes |= NES_UP;
    }
    if (buttons & SCE_CTRL_DOWN) {
        nes |= NES_DOWN;
    }
    if (buttons & SCE_CTRL_LEFT) {
        nes |= NES_LEFT;
    }
    if (buttons & SCE_CTRL_RIGHT) {
        nes |= NES_RIGHT;
    }

    if (buttons & SCE_CTRL_CROSS) {
        nes |= NES_A;
    }
    if (buttons & SCE_CTRL_CIRCLE) {
        nes |= NES_B;
    }
    if (buttons & SCE_CTRL_START) {
        nes |= NES_START;
    }
    if (buttons & SCE_CTRL_SELECT) {
        nes |= NES_SELECT;
    }

    // Turbo A: Square or L shoulder. Turbo B: Triangle or R shoulder.
    // The pulse runs only while a turbo button is held.
    turboA = (buttons & (SCE_CTRL_SQUARE | SCE_CTRL_LTRIGGER)) != 0;
    turboB = (buttons & (SCE_CTRL_TRIANGLE | SCE_CTRL_RTRIGGER)) != 0;

    if (turboA || turboB) {
        const bool on = (turboPhase % kTurboPeriod) < (kTurboPeriod / 2);
        if (on) {
            if (turboA) {
                nes |= NES_A;
            }
            if (turboB) {
                nes |= NES_B;
            }
        }
        ++turboPhase;
    } else {
        turboPhase = 0;
    }

    return nes;
}

void drawFailureScreen(vita2d_texture* texture) {
    // Solid dark red with a small white block, so a failed load is visibly
    // different from a black screen. Details go to the console log.
    vita2d_set_clear_color(RGBA8(90, 0, 0, 255));
    for (int i = 0; i < 180; ++i) {
        vita2d_start_drawing();
        vita2d_clear_screen();
        if (texture) {
            vita2d_draw_rectangle(kScreenWidth / 2 - 20, kScreenHeight / 2 - 20, 40, 40,
                                  RGBA8(255, 255, 255, 255));
        }
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }
}

} // namespace

int main() {
    // The Vita's default clocks leave a lot of headroom unused; QuickNES is
    // light, but 444/222/222 keeps frame pacing solid.
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    if (vita2d_init() < 0) {
        sceKernelExitProcess(0);
        return 0;
    }
    vita2d_set_clear_color(RGBA8(0, 0, 0, 255));
    vita2d_set_vblank_wait(1);

    Emulator emulator;
    vita2d_texture* texture = vita2d_create_empty_texture_format(
        Emulator::width(), Emulator::height(), SCE_GXM_TEXTURE_FORMAT_A8B8G8R8);

    if (!emulator.load(smb2_rom, smb2_rom_size) || !texture) {
        drawFailureScreen(texture);
        if (texture) {
            vita2d_free_texture(texture);
        }
        vita2d_fini();
        sceKernelExitProcess(0);
        return 0;
    }

    // Linear filtering handles the non-integer 4:3 scale nicely.
    vita2d_texture_set_filters(texture, SCE_GXM_TEXTURE_FILTER_LINEAR,
                               SCE_GXM_TEXTURE_FILTER_LINEAR);

    AudioOutput audio;
    audio.start();

    static std::int16_t samples[4096];
    const int textureStride = static_cast<int>(vita2d_texture_get_stride(texture));

    bool turboA = false;
    bool turboB = false;
    int turboPhase = 0;
    int exitHold = 0;

    for (;;) {
        SceCtrlData pad;
        if (sceCtrlPeekBufferPositive(0, &pad, 1) < 0) {
            pad.buttons = 0;
        }

        emulator.setButtons(mapButtons(pad.buttons, turboA, turboB, turboPhase));
        emulator.runFrame();

        const long sampleCount = emulator.readSamples(samples, 4096);
        audio.push(samples, sampleCount, emulator.channels());

        emulator.renderRgba(
            static_cast<std::uint8_t*>(vita2d_texture_get_datap(texture)), textureStride);

        vita2d_start_drawing();
        vita2d_clear_screen();
        vita2d_draw_texture_scale(texture, kOffsetX, 0.0f, kScaleX, kScaleY);
        vita2d_end_drawing();
        vita2d_swap_buffers();

        // Hold Select + Start for a moment to quit cleanly. The PS button and
        // "close application" also works, of course.
        constexpr std::uint32_t kExitCombo = SCE_CTRL_SELECT | SCE_CTRL_START;
        if ((pad.buttons & kExitCombo) == kExitCombo) {
            if (++exitHold >= 90) {
                break;
            }
        } else {
            exitHold = 0;
        }
    }

    audio.stop();
    vita2d_free_texture(texture);
    vita2d_fini();
    sceKernelExitProcess(0);
    return 0;
}
