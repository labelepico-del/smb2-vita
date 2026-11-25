#!/usr/bin/env bash
# Build smb2.nes from the Xkeeper0 disassembly, verify it, and generate the
# embedded C++ array. Requirements: git, python3, sha256sum (coreutils).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="${SMB2_REPO:-https://github.com/Xkeeper0/smb2.git}"
COMMIT="${SMB2_COMMIT:-b73415e1953d7fdffd4957d5fb43fa7a13844c1c}"
WORK="${SMB2_WORKDIR:-${TMPDIR:-/tmp}/smb2-vita-build}"
PRG0="47ba60fad332fdea5ae44b7979fe1ee78de1d316ee027fea2ad5fe3c0d86f25a"

rm -rf "$WORK"
mkdir -p "$WORK"
echo "Cloning $REPO at $COMMIT ..."
git clone --filter=blob:none "$REPO" "$WORK/upstream"
git -C "$WORK/upstream" checkout --detach "$COMMIT"

echo "Assembling ..."
(
    cd "$WORK/upstream"
    chmod +x tools/asm6f tools/asm6f.exe 2>/dev/null || true
    mkdir -p bin
    ./tools/asm6f smb2.asm -n -c -L bin/smb2.nes
)

ACTUAL="$(sha256sum "$WORK/upstream/bin/smb2.nes" | awk '{print $1}')"
echo "SHA-256: $ACTUAL"
if [ "$ACTUAL" != "$PRG0" ]; then
    echo "error: built ROM does not match SMB2 PRG0" >&2
    exit 1
fi

mkdir -p "$ROOT/rom"
cp "$WORK/upstream/bin/smb2.nes" "$ROOT/rom/smb2.nes"
python3 "$ROOT/tools/embed_rom.py" "$ROOT/rom/smb2.nes" "$ROOT/src/rom_data.cpp"
echo "Done. Build the VPK with: cmake -S . -B build && cmake --build build"
