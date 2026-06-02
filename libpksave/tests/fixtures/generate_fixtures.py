#!/usr/bin/env python3
"""
SPDX-License-Identifier: MIT
Copyright (c) 2026 gbsdk contributors

Synthetic Pokemon save fixture generator.

Generates minimal, structurally valid .sav files for each format variant.
These are NOT real saves — they contain known test values at the offsets
the library uses, so tests can assert against predictable values.

TODO(3): fill in the correct byte offsets and field encodings for each
generation as gen1.c through gen5.c are implemented, then update the
expected values in test_genN.c to match what this script embeds.

Usage:
    python3 generate_fixtures.py [output_dir]
"""

import struct
import os
import sys

OUT_DIR = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(__file__)

# ── test values embedded in every fixture ────────────────────────────────────
TEST_MONEY  = 12300
TEST_TID    = 0x1234
TEST_SID    = 0x5678
TEST_BADGES = 0b00001111  # 4 badges
# Trainer name in game charset — TODO(3): use gen-specific encoding
TEST_NAME_ASCII = b"RED\xff"  # 0xFF = end-of-string for Gen I/II

# ── Gen I (Red/Blue, Yellow) — 32 KB ─────────────────────────────────────────
# Offsets from pokered: src/engine/save.asm and include/ram/wram.asm
# TODO(3): verify and complete all offsets from pret/pokered
GEN1_SIZE = 32 * 1024

def make_gen1_rb():
    buf = bytearray(GEN1_SIZE)
    # TODO(3): write trainer name, TID, money, badges at correct offsets
    # Placeholder: mark the buffer so detection can find it once implemented
    buf[0x2598] = 0x01  # TODO(3): real detection signal
    return bytes(buf)

def make_gen1_yellow():
    buf = bytearray(make_gen1_rb())
    # TODO(3): set Pikachu friendship block to distinguish Yellow
    buf[0x2720] = 0x05  # placeholder
    return bytes(buf)

# ── Gen II (Gold/Silver, Crystal) — 32 KB ────────────────────────────────────
# Offsets from pokecrystal: engine/save.asm
# TODO(3): verify from pret/pokecrystal
GEN2_SIZE = 32 * 1024

def make_gen2_gs():
    buf = bytearray(GEN2_SIZE)
    # TODO(3): write trainer data at correct offsets
    return bytes(buf)

def make_gen2_crystal():
    buf = bytearray(make_gen2_gs())
    # Crystal flag byte distinguishes it from GS — TODO(3): correct offset
    buf[0x2009] = 0x01  # placeholder
    return bytes(buf)

# ── Gen III (Ruby/Sapphire, Emerald, FireRed/LeafGreen) — 128 KB ─────────────
# Section-based save with 14 × 4KB sections.
# Game code lives in section 0 at offset 0x00AC (RS), 0x00AC (Emerald, FRLG).
# TODO(3): derive from pokeemerald/src/save.c and include/save.h
GEN3_SIZE = 128 * 1024
GEN3_SECTION_SIZE = 0x1000
GEN3_NUM_SECTIONS = 14

GAME_CODE_RS      = 0x0000
GAME_CODE_EMERALD = 0x0001
GAME_CODE_FRLG    = 0x0002

def make_gen3(game_code: int) -> bytes:
    buf = bytearray(GEN3_SIZE)
    # TODO(3): lay out all 14 sections, write section IDs, game code,
    #          trainer data, and compute Gen III checksums.
    # Placeholder: write game code at section 0 header
    buf[0x00AC] = game_code & 0xFF
    return bytes(buf)

def make_gen3_rs():      return make_gen3(GAME_CODE_RS)
def make_gen3_emerald(): return make_gen3(GAME_CODE_EMERALD)
def make_gen3_frlg():    return make_gen3(GAME_CODE_FRLG)

# ── Gen IV (D/P, Platinum, HG/SS) — 512 KB ───────────────────────────────────
# Footer block at the end of each 0x40000-byte half identifies the game.
# TODO(3): derive from pokeplatinum/src/save.c
GEN4_SIZE = 512 * 1024

FOOTER_DP        = 0xC0F1
FOOTER_PLATINUM  = 0xC0F2
FOOTER_HGSS      = 0xF0F3

def make_gen4(footer_id: int) -> bytes:
    buf = bytearray(GEN4_SIZE)
    # TODO(3): correct footer offset and structure
    struct.pack_into('<H', buf, GEN4_SIZE - 0x14, footer_id)  # placeholder
    return bytes(buf)

def make_gen4_dp():       return make_gen4(FOOTER_DP)
def make_gen4_platinum(): return make_gen4(FOOTER_PLATINUM)
def make_gen4_hgss():     return make_gen4(FOOTER_HGSS)

# ── Gen V (Black/White, B2/W2) — 512 KB ──────────────────────────────────────
# TODO(3): derive from PKHeX for block layout (decomp not complete as of 2025)
GEN5_SIZE = 512 * 1024

FOOTER_BW  = 0xBEEF  # placeholder — TODO(3): real values from PKHeX
FOOTER_BW2 = 0xBEF0

def make_gen5(footer_id: int) -> bytes:
    buf = bytearray(GEN5_SIZE)
    struct.pack_into('<H', buf, GEN5_SIZE - 0x14, footer_id)  # placeholder
    return bytes(buf)

def make_gen5_bw():  return make_gen5(FOOTER_BW)
def make_gen5_bw2(): return make_gen5(FOOTER_BW2)

# ── main ──────────────────────────────────────────────────────────────────────
FIXTURES = {
    "rb.sav":       make_gen1_rb,
    "yellow.sav":   make_gen1_yellow,
    "gs.sav":       make_gen2_gs,
    "crystal.sav":  make_gen2_crystal,
    "rs.sav":       make_gen3_rs,
    "emerald.sav":  make_gen3_emerald,
    "frlg.sav":     make_gen3_frlg,
    "dp.sav":       make_gen4_dp,
    "platinum.sav": make_gen4_platinum,
    "hgss.sav":     make_gen4_hgss,
    "bw.sav":       make_gen5_bw,
    "bw2.sav":      make_gen5_bw2,
}

def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, fn in FIXTURES.items():
        path = os.path.join(OUT_DIR, name)
        data = fn()
        with open(path, "wb") as f:
            f.write(data)
        print(f"  wrote {path} ({len(data)//1024}KB)")

if __name__ == "__main__":
    main()
