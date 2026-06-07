#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
#
# Bake a minimal valid Game Boy Advance cartridge for the mGBA smoke test.
#
# The smoke harness only needs *some* loadable ROM so the mCore can attach
# and the Lua script can introspect `emu`. mGBA's loader checks the header
# checksum and the trailing GBA "fixed byte" (0x96) — neither the Nintendo
# logo nor a real CPU entrypoint is required for the harness to start
# advancing frames. We branch the entry instruction to itself so the CPU
# spins harmlessly while the host calls runFrame().
#
# Layout (from GBA Pan Docs / mGBA's internal/gba/gba.h):
#   0x000..0x004 : ARM branch to start  (B 0xC0)
#   0x004..0x0A0 : Nintendo logo        (zeroed; checksum not enforced for
#                                        emulation, only for the BIOS handshake
#                                        which the harness doesn't go through)
#   0x0A0..0x0AC : game title (12 ASCII bytes)
#   0x0AC..0x0B0 : game code (4 ASCII bytes)
#   0x0B0..0x0B2 : maker code (2 ASCII bytes)
#   0x0B2        : 0x96 fixed value
#   0x0B3        : main unit code (0x00)
#   0x0B4        : device type (0x00)
#   0x0B5..0x0BC : 7 reserved bytes (zero)
#   0x0BC        : software version
#   0x0BD        : header checksum  (x = (-(0x19 + sum(0xA0..0xBC))) & 0xFF)
#   0x0BE..0x0C0 : 2 reserved bytes (zero)
#   0x0C0..     : entrypoint code (branch-to-self)

import sys

def build_rom() -> bytes:
    rom = bytearray(0xC0 + 4)
    # ARM: B 0xC0  →  0xEA00002E (24-bit signed offset = (target - PC - 8) >> 2)
    rom[0x000:0x004] = bytes([0x2E, 0x00, 0x00, 0xEA])
    rom[0x0A0:0x0AC] = b"PKSAVESMOKE "
    rom[0x0AC:0x0B0] = b"SMKE"
    rom[0x0B0:0x0B2] = b"00"
    rom[0x0B2] = 0x96
    # Header checksum over bytes 0xA0 .. 0xBC inclusive.
    chk = 0
    for off in range(0x0A0, 0x0BD):
        chk = (chk - rom[off]) & 0xFF
    chk = (chk - 0x19) & 0xFF
    rom[0x0BD] = chk
    # Entrypoint: B . (infinite branch-to-self, ARM encoding 0xEAFFFFFE)
    rom[0x0C0:0x0C4] = bytes([0xFE, 0xFF, 0xFF, 0xEA])
    return bytes(rom)

def main() -> int:
    if len(sys.argv) != 2:
        print("usage: make-smoke-rom.py <output.gba>", file=sys.stderr)
        return 2
    out = sys.argv[1]
    with open(out, "wb") as f:
        f.write(build_rom())
    return 0

if __name__ == "__main__":
    sys.exit(main())
