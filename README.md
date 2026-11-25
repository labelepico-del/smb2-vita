# SMB2 Vita

A native PlayStation Vita port of [Xkeeper0/smb2](https://github.com/Xkeeper0/smb2),
the commented disassembly of *Super Mario Bros. 2* (NES).

The game is assembled from the disassembly, verified against the original PRG0
ROM, and embedded in the VPK. The [QuickNES](https://github.com/libretro/QuickNES_Core)
core runs it on the Vita, with a small `vita2d` frontend for video, audio and
input.

## How it works

- `tools/build_rom.sh` / `tools/build_rom.ps1` clones the disassembly at a
  pinned commit, assembles it with the included `asm6f`, and checks the
  SHA-256 against SMB2 PRG0 (`47ba60fa...f25a`).
- The ROM is converted to `src/rom_data.cpp` and linked into the executable.
- QuickNES emulates the 6502, PPU, APU and MMC3 (mapper 4) used by the game.
- The frontend renders 256x240 frames through a `vita2d` texture, outputs
  mono QuickNES audio as 48 kHz stereo, and maps the Vita pad to the NES pad.

## Building

### GitHub Actions (recommended)

Push this project to a GitHub repository. The included workflow builds the ROM
from the disassembly, compiles the VPK, and uploads it as an artifact:

1. Open the **Actions** tab and run **Build VPK** (or push a commit).
2. Download the `smb2-vita-vpk` artifact.
3. Install the `.vpk` with VitaShell.

The workflow uses the official `vitasdk/vitasdk` Docker image, so no local Vita
toolchain is needed.

### Locally

Requirements: VitaSDK, CMake 3.16+, and either Python 3 (Linux/macOS) or
PowerShell 5+ (Windows).

```bash
# Linux / macOS
export VITASDK=/path/to/vitasdk
./tools/build_rom.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

```powershell
# Windows
$env:VITASDK = "C:\path\to\vitasdk"
powershell -ExecutionPolicy Bypass -File tools\build_rom.ps1
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The result is `build/smb2_vita.vpk`. If you already have a `smb2.nes`, regenerate
only the embedded array:

```bash
python3 tools/embed_rom.py smb2.nes src/rom_data.cpp
```

`rom/smb2.nes` and `src/rom_data.cpp` are deliberately gitignored: they contain
Nintendo game data.

## Controls

| Vita | NES |
| --- | --- |
| D-pad | D-pad |
| Cross | A |
| Circle | B |
| Square or L | Turbo A |
| Triangle or R | Turbo B |
| Start | Start |
| Select | Select |
| Select + Start (hold ~1.5 s) | Quit cleanly |
| PS button | System menu |

Turbo pulses at 15 Hz (two frames on, two frames off at 60 fps). The game is
scaled to 4:3 and letterboxed.

## Notes

- No BIOS or firmware files are needed.
- SMB2 has no battery save, so there is nothing to persist.
- The core runs at 444 MHz ARM / 222 MHz bus / 222 MHz GPU; QuickNES has ample
  headroom at 60 fps.
- To follow a newer disassembly revision, change `SMB2_COMMIT` in
  `tools/build_rom.sh`, `tools/build_rom.ps1` and
  `.github/workflows/build-vpk.yml`. The ROM hash check will tell you if the
  build is no longer byte-identical to PRG0.

## Legal

This project contains no game data in source control. Building it reproduces
the copyrighted ROM from the disassembly; keep builds for personal use and do
not redistribute the VPK. *Super Mario Bros. 2* is a trademark of Nintendo. This
is an unofficial homebrew project, not affiliated with Nintendo or Sony.

## Credits

- [Xkeeper0/smb2](https://github.com/Xkeeper0/smb2) — the disassembly.
- [QuickNES](https://github.com/libretro/QuickNES_Core) by Shay Green (blargg)
  and contributors — NES emulation core (LGPL-2.1).
- [VitaSDK](https://vitasdk.org), `vita2d` and the Vita homebrew community.
