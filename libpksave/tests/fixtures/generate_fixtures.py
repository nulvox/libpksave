#!/usr/bin/env python3
"""
SPDX-License-Identifier: MIT
Copyright (c) 2026 gbsdk contributors

Synthetic Pokemon save fixture generator.

Produces minimal structurally-valid .sav files for each format variant.
All offsets and algorithms derived from pret decomp projects.

Usage:
    python3 generate_fixtures.py [output_dir]
"""

import struct
import os
import sys

OUT_DIR = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(__file__)

# ── shared test values ────────────────────────────────────────────────────────
TEST_MONEY      = 12300
TEST_TID        = 0x1234
TEST_SID        = 0x5678
TEST_BADGES     = 0x0F          # lower 4 bits set = 4 badges

# Party slot 0: species_id, level, first_move_id
#   Gen I/II internal species ID for Bulbasaur = 0x99 (153)
#   Gen III+: national dex 1 = Bulbasaur
GEN12_SPECIES   = 0x99          # Gen I/II internal ID for Bulbasaur
GEN3P_SPECIES   = 1             # Gen III+ national dex for Bulbasaur
TEST_LEVEL      = 5
TEST_MOVE1      = 33            # Tackle (move #33 in all gens)

# ── Gen I character encoding ──────────────────────────────────────────────────
# Uppercase A-Z: 0x80-0x99. Lowercase a-z: 0xA0-0xB9.
# End-of-string: 0x50. Space: 0x7F.

def gen1_encode(s, length=11):
    """Encode ASCII string to Gen I/II charset, padded/terminated with 0x50."""
    out = []
    for c in s:
        if 'A' <= c <= 'Z':
            out.append(0x80 + ord(c) - ord('A'))
        elif 'a' <= c <= 'z':
            out.append(0xA0 + ord(c) - ord('a'))
        elif c == ' ':
            out.append(0x7F)
        elif c == "'":
            out.append(0xE0)
        elif c == '-':
            out.append(0xE3)
        elif '0' <= c <= '9':
            out.append(0xF6 + ord(c) - ord('0'))
        else:
            out.append(0x50)  # unknown → terminator
    # Pad to length with 0x50
    while len(out) < length:
        out.append(0x50)
    return bytes(out[:length])

# ── Gen III character encoding ────────────────────────────────────────────────
# Uppercase A-Z: 0xA1-0xBA. Lowercase a-z: 0xBB-0xD4.
# End-of-string: 0xFF. Space: 0x00.

def gen3_encode(s, length=7):
    """Encode ASCII string to Gen III charset."""
    out = []
    for c in s:
        if 'A' <= c <= 'Z':
            out.append(0xA1 + ord(c) - ord('A'))
        elif 'a' <= c <= 'z':
            out.append(0xBB + ord(c) - ord('a'))
        elif c == ' ':
            out.append(0x00)
        else:
            out.append(0xFF)
    while len(out) < length:
        out.append(0xFF)
    return bytes(out[:length])

# ── Gen IV/V character encoding ────────────────────────────────────────────────
# UTF-16LE, 0xFFFF terminator, max 8 characters (16 bytes).

def gen45_encode(s, length=16):
    """Encode ASCII string to Gen IV/V UTF-16LE format."""
    out = bytearray()
    for c in s:
        out += struct.pack('<H', ord(c))
    out += b'\xFF\xFF'  # 0xFFFF terminator
    while len(out) < length:
        out += b'\x00\x00'
    return bytes(out[:length])

# ── BCD helpers ────────────────────────────────────────────────────────────────

def bcd_encode(value, nbytes):
    """Encode integer to packed BCD, big-endian, nbytes wide."""
    digits = f'{value:0{nbytes*2}d}'
    out = []
    for i in range(0, len(digits), 2):
        out.append(int(digits[i]) * 16 + int(digits[i+1]))
    return bytes(out)

# ── Gen I checksum ────────────────────────────────────────────────────────────
# Algorithm: sum bytes from 0x2598 to 0x3522, negate (one's complement), store at 0x3523.
# From pokered/engine/save_menu.asm.

GEN1_CHKSUM_START = 0x2598
GEN1_CHKSUM_END   = 0x3522   # inclusive
GEN1_CHKSUM_ADDR  = 0x3523

def gen1_checksum(buf):
    total = 0
    for b in buf[GEN1_CHKSUM_START : GEN1_CHKSUM_END + 1]:
        total += b
    return (~total) & 0xFF

# ── Gen II checksum ───────────────────────────────────────────────────────────
# Primary 16-bit sum of range, stored little-endian.
# GS:      sum 0x2009-0x2D68 → store at 0x2D69 (LE uint16)
# Crystal: sum 0x2009-0x2B82 → store at 0x2D0D (LE uint16)
#          secondary: sum 0x1209-0x1D82 → store at 0x1F0D (LE uint16)

def gen2_checksum_range(buf, start, end):
    return sum(buf[start:end+1]) & 0xFFFF

# ── Gen III checksum ──────────────────────────────────────────────────────────
# 32-bit word sum of section data, then fold upper 16 into lower 16.
# Section data size varies per section ID.

GEN3_SECTION_SIZES = {
    0: 3884, 1: 3968, 2: 3968, 3: 3968,
    4: 3848, 5: 3968, 6: 3968, 7: 3968,
    8: 3968, 9: 3968, 10: 3968, 11: 3968,
    12: 3968, 13: 2000,
}
GEN3_SECTION_SIG   = 0x08012025
GEN3_SECTION_SIZE  = 0x1000

def gen3_section_checksum(data, section_id):
    size = GEN3_SECTION_SIZES.get(section_id, 3968)
    total = 0
    for i in range(0, size, 4):
        word = struct.unpack_from('<I', data, i)[0] if i + 4 <= len(data) else 0
        total = (total + word) & 0xFFFFFFFF
    return ((total >> 16) + (total & 0xFFFF)) & 0xFFFF

# ── Gen IV CRC-16-CCITT ───────────────────────────────────────────────────────

def crc16_ccitt(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

# ── Gen IV/V PRNG for Pokémon data encryption ─────────────────────────────────

def prng_next(seed):
    return (0x41C64E6D * seed + 0x6073) & 0xFFFFFFFF

def gen45_encrypt_substructs(data_32bytes_x4, key):
    """Encrypt 128 bytes of Pokémon substruct data using PRNG(key)."""
    result = bytearray(128)
    seed = key
    for i in range(0, 128, 2):
        seed = prng_next(seed)
        enc_word = seed >> 16
        word = struct.unpack_from('<H', data_32bytes_x4, i)[0]
        struct.pack_into('<H', result, i, word ^ enc_word)
    return bytes(result)

def gen45_compute_checksum(decrypted_substructs):
    """16-bit sum of all uint16 words in the 128-byte substructs."""
    total = 0
    for i in range(0, 128, 2):
        total += struct.unpack_from('<H', decrypted_substructs, i)[0]
    return total & 0xFFFF

# ── Gen III Pokémon struct helpers ────────────────────────────────────────────
# Box mon = 80 bytes, party mon = 100 bytes (box + 20 extra bytes).
# Substructure order determined by PID % 24 (standard permutation table).
# For PID=0: order is GAEM (Growth=0, Attacks=1, EVs=2, Misc=3).

GEN3_SUBORDER = [
    [0,1,2,3],[0,1,3,2],[0,2,1,3],[0,2,3,1],[0,3,1,2],[0,3,2,1],
    [1,0,2,3],[1,0,3,2],[1,2,0,3],[1,2,3,0],[1,3,0,2],[1,3,2,0],
    [2,0,1,3],[2,0,3,1],[2,1,0,3],[2,1,3,0],[2,3,0,1],[2,3,1,0],
    [3,0,1,2],[3,0,2,1],[3,1,0,2],[3,1,2,0],[3,2,0,1],[3,2,1,0],
]

def gen3_make_party_mon(species, level, move1, ot_tid, ot_sid, pid=0):
    """Build a 100-byte Gen III party Pokémon struct."""
    order = GEN3_SUBORDER[pid % 24]

    # Build 4 unencrypted 12-byte substructs
    subs = [bytearray(12) for _ in range(4)]

    # Substruct 0 (Growth): species(2), item(2), exp(4), pp_bonuses(1), friendship(1), _pad(2)
    struct.pack_into('<H', subs[0], 0, species)   # species
    struct.pack_into('<I', subs[0], 4, level * 10) # rough exp

    # Substruct 1 (Attacks): move1..4 (2 each), pp1..4 (1 each)
    struct.pack_into('<H', subs[1], 0, move1)  # move 1
    subs[1][8] = 35   # PP for move 1 (Tackle PP = 35)

    # Substruct 2 (EVs): HP,Atk,Def,Spd,SpA,SpD EVs (1 byte each)
    # all zero = fine

    # Substruct 3 (Misc): IVs packed in 4 bytes; shininess comes from IVs
    # Use IV=10 for all stats (not shiny by Gen III rules)
    # IVs packed: bits 0-4=HP, 5-9=Atk, 10-14=Def, 15-19=Spd, 20-24=SpA, 25-29=SpD
    iv_word = (10) | (10 << 5) | (10 << 10) | (10 << 15) | (10 << 20) | (10 << 25)
    struct.pack_into('<I', subs[3], 0, iv_word)

    # Arrange substructs in PID-determined order
    arranged = bytearray(48)
    for i, sub_idx in enumerate(order):
        arranged[i*12:(i+1)*12] = subs[sub_idx]

    # XOR key = PID XOR (TID | (SID<<16))
    key = pid ^ (ot_tid | (ot_sid << 16))
    encrypted = bytearray(48)
    seed = key
    for i in range(0, 48, 4):
        seed = (0x41C64E6D * seed + 0x6073) & 0xFFFFFFFF
        enc = seed
        word = struct.unpack_from('<I', arranged, i)[0]
        struct.pack_into('<I', encrypted, i, word ^ enc)

    # Compute checksum: sum of uint16 words in UNENCRYPTED substructs
    checksum = 0
    for i in range(0, 48, 2):
        checksum = (checksum + struct.unpack_from('<H', arranged, i)[0]) & 0xFFFF

    # Build box mon (80 bytes)
    box = bytearray(80)
    struct.pack_into('<I', box, 0x00, pid)          # PID
    struct.pack_into('<H', box, 0x04, ot_tid)       # TID
    struct.pack_into('<H', box, 0x06, ot_sid)       # SID
    # nickname at 0x08 (10 bytes, Gen III charset) — use encoded species name or placeholder
    box[0x08:0x12] = gen3_encode("BULBASAUR", 10)
    box[0x12] = 0x02  # language = English
    box[0x13] = 0x00  # flags
    box[0x14:0x1B] = gen3_encode("RED", 7)           # OT name
    box[0x1C:0x1E] = struct.pack('<H', checksum)    # checksum
    box[0x20:0x50] = encrypted                       # encrypted substructs

    # Party mon = box mon + 20 extra bytes
    party = bytearray(100)
    party[:80] = box
    party[0x50:0x52] = struct.pack('<H', 0)          # status
    party[0x52] = level                              # level
    party[0x53] = 0                                  # hold mail
    # Stats: HP, Atk, Def, Spd, SpA, SpD (2 bytes each = 12 bytes at 0x54)
    party[0x54:0x56] = struct.pack('<H', 20)   # current HP
    party[0x56:0x58] = struct.pack('<H', 20)   # max HP
    party[0x58:0x5A] = struct.pack('<H', 11)   # Atk
    party[0x5A:0x5C] = struct.pack('<H', 11)   # Def
    party[0x5C:0x5E] = struct.pack('<H', 9)    # Spd
    party[0x5E:0x60] = struct.pack('<H', 8)    # SpA
    party[0x60:0x62] = struct.pack('<H', 8)    # SpD
    return bytes(party)

# ── Gen IV Pokémon struct helpers ─────────────────────────────────────────────
# Box mon = 136 bytes, party mon = 236 bytes.

def gen4_make_party_mon(species, level, move1, ot_tid, ot_sid, pid=0):
    """Build a 236-byte Gen IV party Pokémon struct."""
    # Build 4 unencrypted 32-byte subblocks
    subs = [bytearray(32) for _ in range(4)]

    # Block A (Growth): species(2), item(2), exp(4), friendship(1), ability(1), markings(1), lang(1), EVs(6), contest stats(6)
    struct.pack_into('<H', subs[0], 0, species)
    struct.pack_into('<I', subs[0], 4, level * 10)
    subs[0][8] = 70    # friendship
    subs[0][9] = 1     # ability slot
    subs[0][11] = 0x09 # language = English

    # Block B (Attacks): moves 1-4 (2 each), pp 1-4 (1 each), pp_ups 1-4 (1 each)
    struct.pack_into('<H', subs[1], 0, move1)
    subs[1][8] = 35    # PP for move 1

    # Block C (EVs): HP(1),Atk(1),Def(1),Spd(1),SpA(1),SpD(1),... rest unused
    # all zero

    # Block D (Misc): IVs packed (4 bytes), flags(4), poke-ball, met level+loc, etc.
    # IVs: bits 0-4=HP, 5-9=Atk, 10-14=Def, 15-19=Spd, 20-24=SpA, 25-29=SpD, bit30=egg, bit31=nick'd
    iv_word = (15)|(15<<5)|(15<<10)|(15<<15)|(15<<20)|(15<<25)
    struct.pack_into('<I', subs[3], 0, iv_word)
    subs[3][4] = level & 0x7F  # met level (packed with other stuff)

    # Concatenate subblocks in PID%24 order
    order = GEN3_SUBORDER[pid % 24]
    arranged = bytearray(128)
    for i, idx in enumerate(order):
        arranged[i*32:(i+1)*32] = subs[idx]

    # Compute checksum (sum of uint16 words in decrypted substructs)
    checksum = gen45_compute_checksum(bytes(arranged))

    # Encrypt with PRNG seeded by checksum
    encrypted = gen45_encrypt_substructs(bytes(arranged), checksum)

    # Build box mon (136 bytes)
    box = bytearray(136)
    struct.pack_into('<I', box, 0x00, pid)
    struct.pack_into('<H', box, 0x04, 0xAFFE)     # unused/ability flag
    struct.pack_into('<H', box, 0x06, checksum)
    box[0x08:0x88] = encrypted

    # Nickname at 0x48 (20 bytes = 10 UTF-16LE chars)
    nickname_offset = 0x48
    nick = gen45_encode("BULBASAUR", 20)
    box[nickname_offset:nickname_offset+20] = nick

    # OT name at 0x68 (16 bytes = 8 UTF-16LE chars)
    ot_offset = 0x68
    ot = gen45_encode("RED", 16)
    box[ot_offset:ot_offset+16] = ot

    # Misc bytes
    box[0x40] = 0x02   # language = English
    box[0x41] = 0x00   # markings
    struct.pack_into('<H', box, 0x42, ot_tid)
    struct.pack_into('<H', box, 0x44, ot_sid)

    # Party mon = box mon + 100 extra bytes
    party = bytearray(236)
    party[:136] = box
    # Status at 0x88
    struct.pack_into('<H', party, 0x88, 0)   # no status
    party[0x8C] = level
    struct.pack_into('<H', party, 0x8E, 20)  # current HP
    struct.pack_into('<H', party, 0x90, 20)  # max HP
    struct.pack_into('<H', party, 0x92, 11)  # Atk
    struct.pack_into('<H', party, 0x94, 11)  # Def
    struct.pack_into('<H', party, 0x96, 9)   # Spd
    struct.pack_into('<H', party, 0x98, 8)   # SpA
    struct.pack_into('<H', party, 0x9A, 8)   # SpD
    return bytes(party)

# ── Gen I (Red/Blue, Yellow) — 32 KB ─────────────────────────────────────────
# Offsets from pret/pokered ram/sram.asm and engine/save_menu.asm
# sPlayerName at sram+0x598 → .sav 0x2598 (bank1 start + 0x598)
# Money (BCD) at 0x25F3, Badges at 0x2602, TID at 0x2605 (big-endian)
# Party count at 0x2F2C, species list at 0x2F2D (6+0xFF),
# party data at 0x2F34 (6×44 bytes), OT names at 0x303C, mon names at 0x307E
# Checksum: 0xFF - sum(0x2598..0x3522) stored at 0x3523

GEN1_SIZE = 32 * 1024

# Gen I/II party mon offsets within 44-byte struct
# 0x00: species, 0x01: HP, 0x03: box_level, 0x04: status
# 0x05: types(2), 0x07: catch_rate, 0x08: moves(4),
# 0x0C: OT_ID(2), 0x0E: exp(3), 0x11: stat_exp(10), 0x1B: DVs(2),
# 0x1D: PP(4), 0x21: level, 0x22: stats(10)

def gen1_make_party_mon(species, level, move1):
    mon = bytearray(44)
    mon[0x00] = species
    mon[0x03] = level    # box_level (catch_rate slot reused as level in party)
    mon[0x08] = move1    # first move
    mon[0x0C:0x0E] = struct.pack('>H', TEST_TID)   # OT ID big-endian
    # exp: set to something appropriate for level 5 (~35 for most mons)
    exp = 35
    mon[0x0E] = (exp >> 16) & 0xFF
    mon[0x0F] = (exp >> 8) & 0xFF
    mon[0x10] = exp & 0xFF
    # DVs: Atk=9, Def=8, Spd=8, Spc=8 (→ not shiny)
    dvs = (9 << 12) | (8 << 8) | (8 << 4) | 8
    struct.pack_into('>H', mon, 0x1B, dvs)
    mon[0x1D] = 35   # PP for move 1 (Tackle has 35 PP)
    mon[0x21] = level
    # Stats: max_hp(2), atk(2), def(2), spd(2), spc(2)
    struct.pack_into('>H', mon, 0x22, 20)   # max HP
    struct.pack_into('>H', mon, 0x24, 11)   # Atk
    struct.pack_into('>H', mon, 0x26, 9)    # Def
    struct.pack_into('>H', mon, 0x28, 8)    # Spd
    struct.pack_into('>H', mon, 0x2A, 8)    # Spc
    return bytes(mon)

# Gen I/II box mon: first 33 bytes of the party struct (no level/stats).
# Level is read from data[0x03] (box_level field).
def gen1_make_box_mon(species, level, move1):
    return gen1_make_party_mon(species, level, move1)[:33]

GEN1_BOX_SIZE = 1122        # 1 count + 21 species + 20*33 + 20*11 + 20*11
GEN1_BOX_MON_SIZE = 33
GEN1_NAME_LEN = 11

def gen1_write_box(buf, base_off, mons):
    """Populate a Gen I PC box (1122 bytes) with the given list of (species, level, move) tuples."""
    n = len(mons)
    buf[base_off] = n
    # species list: n entries followed by 0xFF terminator, padded with 0xFF
    for i, (sp, _, _) in enumerate(mons):
        buf[base_off + 1 + i] = sp
    buf[base_off + 1 + n] = 0xFF
    for i in range(n + 1, 21):
        buf[base_off + 1 + i] = 0xFF
    data_off  = base_off + 22
    ot_off    = data_off + 20 * GEN1_BOX_MON_SIZE
    nick_off  = ot_off  + 20 * GEN1_NAME_LEN
    for i, (sp, lvl, mv) in enumerate(mons):
        buf[data_off + i*GEN1_BOX_MON_SIZE : data_off + (i+1)*GEN1_BOX_MON_SIZE] = \
            gen1_make_box_mon(sp, lvl, mv)
        buf[ot_off   + i*GEN1_NAME_LEN : ot_off   + (i+1)*GEN1_NAME_LEN] = \
            gen1_encode("RED", GEN1_NAME_LEN)
        buf[nick_off + i*GEN1_NAME_LEN : nick_off + (i+1)*GEN1_NAME_LEN] = \
            gen1_encode("BULBASAUR", GEN1_NAME_LEN)

def make_gen1_rb():
    buf = bytearray(GEN1_SIZE)

    # Player name at 0x2598 (11 bytes)
    buf[0x2598:0x25A3] = gen1_encode("RED", 11)

    # Money (BCD, 3 bytes) at 0x25F3
    buf[0x25F3:0x25F6] = bcd_encode(TEST_MONEY, 3)

    # Badges at 0x2602
    buf[0x2602] = TEST_BADGES

    # TID at 0x2605 (big-endian uint16)
    struct.pack_into('>H', buf, 0x2605, TEST_TID)

    # Party: 1 Pokémon (Bulbasaur)
    buf[0x2F2C] = 1                         # party count
    buf[0x2F2D] = GEN12_SPECIES             # species[0]
    buf[0x2F2E:0x2F34] = bytes([0xFF]*6)    # remaining species slots
    buf[0x2F33] = 0xFF                      # terminator (slot after last)

    # Actually: species list = 6 slots + 1 terminator
    # 0x2F2D: species[0], 0x2F2E-0x2F32: species[1-5], 0x2F33: 0xFF terminator
    buf[0x2F2D] = GEN12_SPECIES
    buf[0x2F2E] = 0xFF   # species[1] = 0xFF (empty)
    buf[0x2F2F] = 0xFF
    buf[0x2F30] = 0xFF
    buf[0x2F31] = 0xFF
    buf[0x2F32] = 0xFF
    buf[0x2F33] = 0xFF   # terminator

    # Party data at 0x2F34
    mon = gen1_make_party_mon(GEN12_SPECIES, TEST_LEVEL, TEST_MOVE1)
    buf[0x2F34:0x2F34+44] = mon

    # OT names at 0x303C (6×11 bytes)
    buf[0x303C:0x303C+11] = gen1_encode("RED", 11)

    # Mon names at 0x307E (6×11 bytes)
    # Bulbasaur's name in Gen 1 charset
    buf[0x307E:0x307E+11] = gen1_encode("BULBASAUR", 11)

    # Current box number at 0x284C
    buf[0x284C] = 0   # box 1

    # Populate box 1 (stored in bank 2 at 0x4000 + 1*1122) with one Bulbasaur lvl 7
    gen1_write_box(buf, 0x4000 + 1 * GEN1_BOX_SIZE,
                   [(GEN12_SPECIES, 7, TEST_MOVE1)])

    # Checksum
    buf[GEN1_CHKSUM_ADDR] = gen1_checksum(buf)
    return bytes(buf)

def make_gen1_yellow():
    buf = bytearray(make_gen1_rb())
    # Yellow marker: Pikachu friendship byte at 0x2720 (non-zero = Yellow)
    buf[0x2720] = 0x05
    # Recompute checksum
    buf[GEN1_CHKSUM_ADDR] = gen1_checksum(buf)
    return bytes(buf)

# ── Gen II (Gold/Silver, Crystal) — 32 KB ────────────────────────────────────
# Offsets from pret/pokecrystal engine/save.asm
# TID at 0x2008 (BE uint16), name at 0x200B (11 bytes)
# GS money at 0x23DB (BCD 3 bytes), GS badges at 0x23E4
# Crystal money at 0x23DC, Crystal badges at 0x23E5
# GS party count at 0x288A, Crystal at 0x2865
# GS party species at 0x288B, Crystal at 0x2866
# GS party data at 0x2892 (6×48 bytes), Crystal at 0x286D
# GS checksum: sum(0x2009..0x2D68) → 0x2D69 (LE uint16)
# Crystal primary: sum(0x2009..0x2B82) → 0x2D0D; secondary: sum(0x1209..0x1D82) → 0x1F0D

GEN2_SIZE = 32 * 1024

GEN2_BOX_SIZE = 1102        # 1 + 21 + 20*32 + 20*11 + 20*11
GEN2_BOX_MON_SIZE = 32
GEN2_NAME_LEN = 11
GEN2_BOX_BANK2 = 0x4000     # boxes 0-6
GEN2_BOX_BANK3 = 0x6000     # boxes 7-13

def gen2_make_box_mon(species, level, move1):
    """Gen II box mon = first 32 bytes of party mon struct (no level/stats)."""
    return gen2_make_party_mon(species, level, move1)[:GEN2_BOX_MON_SIZE]

def gen2_box_base(box):
    return (GEN2_BOX_BANK2 + box * GEN2_BOX_SIZE) if box < 7 \
        else (GEN2_BOX_BANK3 + (box - 7) * GEN2_BOX_SIZE)

def gen2_write_box(buf, base_off, mons):
    """Populate a Gen II PC box (1102 bytes) with the given list of (species, level, move) tuples."""
    n = len(mons)
    buf[base_off] = n
    for i, (sp, _, _) in enumerate(mons):
        buf[base_off + 1 + i] = sp
    buf[base_off + 1 + n] = 0xFF
    for i in range(n + 1, 21):
        buf[base_off + 1 + i] = 0xFF
    data_off = base_off + 22
    ot_off   = data_off + 20 * GEN2_BOX_MON_SIZE
    nick_off = ot_off  + 20 * GEN2_NAME_LEN
    for i, (sp, lvl, mv) in enumerate(mons):
        buf[data_off + i*GEN2_BOX_MON_SIZE : data_off + (i+1)*GEN2_BOX_MON_SIZE] = \
            gen2_make_box_mon(sp, lvl, mv)
        buf[ot_off   + i*GEN2_NAME_LEN : ot_off   + (i+1)*GEN2_NAME_LEN] = \
            gen1_encode("RED", GEN2_NAME_LEN)
        buf[nick_off + i*GEN2_NAME_LEN : nick_off + (i+1)*GEN2_NAME_LEN] = \
            gen1_encode("BULBASAUR", GEN2_NAME_LEN)

def gen2_make_party_mon(species, level, move1):
    """Build a 48-byte Gen II party mon struct."""
    mon = bytearray(48)
    mon[0x00] = species
    mon[0x01] = 0        # held item (none)
    mon[0x02] = move1    # move 1
    mon[0x03] = 0        # move 2
    mon[0x04] = 0        # move 3
    mon[0x05] = 0        # move 4
    struct.pack_into('>H', mon, 0x06, TEST_TID)   # OT ID
    # exp: 35 for level 5
    exp = 35
    mon[0x08] = (exp >> 16) & 0xFF
    mon[0x09] = (exp >> 8) & 0xFF
    mon[0x0A] = exp & 0xFF
    # stat exp: all 0 (10 bytes at 0x0B)
    # DVs at 0x15: same format as Gen I
    dvs = (9 << 12) | (8 << 8) | (8 << 4) | 8
    struct.pack_into('>H', mon, 0x15, dvs)
    mon[0x17] = 35   # PP for move 1
    mon[0x1B] = 70   # friendship
    mon[0x1C] = 0    # pokerus strain
    mon[0x1D] = 0    # pokerus duration
    # caught data at 0x1E-0x1F
    mon[0x1E] = 0    # caught data (no special)
    mon[0x1F] = 0
    mon[0x20] = level  # level
    mon[0x21] = 0      # status
    struct.pack_into('>H', mon, 0x22, 20)   # current HP
    # Stats at 0x24: max_hp, atk, def, spd, spa, spd (6×2 = 12 bytes)
    struct.pack_into('>H', mon, 0x24, 20)   # max HP
    struct.pack_into('>H', mon, 0x26, 11)   # Atk
    struct.pack_into('>H', mon, 0x28, 9)    # Def
    struct.pack_into('>H', mon, 0x2A, 8)    # Spd
    struct.pack_into('>H', mon, 0x2C, 8)    # SpA (same as Spc)
    struct.pack_into('>H', mon, 0x2E, 8)    # SpD (same as SpA in Gen II)
    return bytes(mon)

def make_gen2_base(party_count_off, party_species_off, party_data_off,
                   money_off, badges_off):
    buf = bytearray(GEN2_SIZE)

    # TID at 0x2008 (big-endian uint16)
    struct.pack_into('>H', buf, 0x2008, TEST_TID)

    # Player name at 0x200B (11 bytes, Gen2 charset = same as Gen1)
    buf[0x200B:0x2016] = gen1_encode("RED", 11)

    # Money (BCD 3 bytes)
    buf[money_off:money_off+3] = bcd_encode(TEST_MONEY, 3)

    # Badges
    buf[badges_off] = TEST_BADGES

    # Party: 1 Pokémon
    buf[party_count_off] = 1
    buf[party_species_off] = GEN12_SPECIES
    buf[party_species_off+1] = 0xFF   # terminator

    mon = gen2_make_party_mon(GEN12_SPECIES, TEST_LEVEL, TEST_MOVE1)
    buf[party_data_off:party_data_off+48] = mon

    return buf

def make_gen2_gs():
    buf = make_gen2_base(
        party_count_off=0x288A, party_species_off=0x288B, party_data_off=0x2892,
        money_off=0x23DB, badges_off=0x23E4
    )
    # Populate box 1 (stored, bank 2) with one Bulbasaur lvl 8
    gen2_write_box(buf, gen2_box_base(1),
                   [(GEN12_SPECIES, 8, TEST_MOVE1)])
    # Primary checksum: sum bytes 0x2009..0x2D68
    chk1 = gen2_checksum_range(buf, 0x2009, 0x2D68)
    struct.pack_into('<H', buf, 0x2D69, chk1)
    return bytes(buf)

def make_gen2_crystal():
    buf = make_gen2_base(
        party_count_off=0x2865, party_species_off=0x2866, party_data_off=0x286D,
        money_off=0x23DC, badges_off=0x23E5
    )
    # Populate box 7 (stored, bank 3) with one Ivysaur (0x9A) for cross-bank
    # coverage of gen2_get_box
    gen2_write_box(buf, gen2_box_base(7),
                   [(0x9A, 16, TEST_MOVE1)])
    # Crystal primary checksum: sum bytes 0x2009..0x2B82 → store at 0x2D0D
    chk1 = gen2_checksum_range(buf, 0x2009, 0x2B82)
    struct.pack_into('<H', buf, 0x2D0D, chk1)
    # Crystal secondary: sum 0x1209..0x1D82 → 0x1F0D
    chk2 = gen2_checksum_range(buf, 0x1209, 0x1D82)
    struct.pack_into('<H', buf, 0x1F0D, chk2)
    return bytes(buf)

# ── Gen III (Ruby/Sapphire, Emerald, FireRed/LeafGreen) — 128 KB ─────────────
# 14 sections × 4096 bytes per section, 2 slots (slot 0 = sections 0-13).
# Section footer: ID@0xFF4(2), checksum@0xFF6(2), sig@0xFF8(4), count@0xFFC(4)
# Section 0 (Trainer/SaveBlock2): name@0x00(7), gender@0x08, TID@0x0A, SID@0x0C
# Game code at section 0 offset 0x00AC:
#   RS: 0x00000000, Emerald: non-zero security key, FRLG: 0x00000001
# Security key also used for money XOR (RS: none, E: key@sec0+0x00AC, FRLG: key@sec0+0x0AF8)
# Section 1 (Team/Items/SaveBlock1): RS/E team_size@0x234, team@0x238, money@0x490
#   FRLG team_size@0x34, team@0x38, money@0x290

GEN3_SIZE = 128 * 1024
GEN3_NUM_SECTIONS = 14

def gen3_make_section(section_id, data, save_count):
    """Build a 4096-byte Gen III section."""
    sec = bytearray(GEN3_SIZE // GEN3_NUM_SECTIONS)  # 4096 bytes
    data_size = GEN3_SECTION_SIZES.get(section_id, 3968)
    sec[:len(data)] = data[:data_size]
    # Footer
    checksum = gen3_section_checksum(sec, section_id)
    struct.pack_into('<H', sec, 0xFF4, section_id)
    struct.pack_into('<H', sec, 0xFF6, checksum)
    struct.pack_into('<I', sec, 0xFF8, GEN3_SECTION_SIG)
    struct.pack_into('<I', sec, 0xFFC, save_count)
    return bytes(sec)

def gen3_make_section0(trainer_name_bytes, tid, sid, game_code_or_key, security_key_frlg=0):
    """Build section 0 data (SaveBlock2 / Trainer Info)."""
    data = bytearray(3884)
    data[0x0000:0x0007] = trainer_name_bytes[:7]
    data[0x0008] = 0   # gender: male
    struct.pack_into('<H', data, 0x000A, tid)
    struct.pack_into('<H', data, 0x000C, sid)
    # game code / security key at 0x00AC
    struct.pack_into('<I', data, 0x00AC, game_code_or_key)
    # FRLG security key at 0x0AF8
    struct.pack_into('<I', data, 0x0AF8, security_key_frlg)
    return bytes(data)

def gen3_make_section1_rse(money, security_key, party_mon_bytes):
    """Build section 1 data (SaveBlock1) for RS/E layout."""
    data = bytearray(3968)
    # Team size at 0x234
    data[0x234] = 1
    # Team data at 0x238 (100 bytes per mon)
    data[0x238:0x238+100] = party_mon_bytes[:100]
    # Money at 0x490 (XORed with security key)
    money_enc = money ^ security_key
    struct.pack_into('<I', data, 0x490, money_enc)
    # Badges at 0x4? (varies) — simplified: put TEST_BADGES somewhere accessible
    # From pokeemerald SaveBlock2: badges are in section 0 at a specific offset.
    # For simplicity, we'll store badges in section 4 or a separate field.
    # Actually badges in Gen III are stored in SaveBlock2 (section 0).
    return bytes(data)

def gen3_make_section1_frlg(money, security_key, party_mon_bytes):
    """Build section 1 data (SaveBlock1) for FRLG layout."""
    data = bytearray(3968)
    data[0x034] = 1   # team size
    data[0x038:0x038+100] = party_mon_bytes[:100]
    money_enc = money ^ security_key
    struct.pack_into('<I', data, 0x290, money_enc)
    return bytes(data)

GEN3_BOX_MON_SIZE = 80
GEN3_SLOTS_PER_BOX = 30
GEN3_NUM_BOXES = 14
GEN3_PC_SEC_FIRST = 5
GEN3_PC_SEC_LAST  = 13
GEN3_PC_SEC_DATA  = 3968   # sections 5..12
GEN3_PC_SEC13_DATA = 2000  # section 13

def gen3_pc_write(sections_data, lin_off, payload):
    """Write `payload` to the linear PC-storage byte stream that spans
    sections 5..13. `sections_data` is a list of mutable bytearrays."""
    p = 0
    while p < len(payload):
        if lin_off < 8 * GEN3_PC_SEC_DATA:
            sec_idx = GEN3_PC_SEC_FIRST + (lin_off // GEN3_PC_SEC_DATA)
            within  = lin_off %  GEN3_PC_SEC_DATA
            sec_sz  = GEN3_PC_SEC_DATA
        else:
            sec_idx = GEN3_PC_SEC_LAST
            within  = lin_off - 8 * GEN3_PC_SEC_DATA
            sec_sz  = GEN3_PC_SEC13_DATA
        avail = sec_sz - within
        take  = min(avail, len(payload) - p)
        buf   = sections_data[sec_idx]
        buf[within:within+take] = payload[p:p+take]
        sections_data[sec_idx] = buf
        p       += take
        lin_off += take

def gen3_make_box_mon(species, level, move1, ot_tid, ot_sid, pid=0x12345678):
    """First 80 bytes of a Gen III party mon = the box mon."""
    return gen3_make_party_mon(species, level, move1, ot_tid, ot_sid, pid)[:GEN3_BOX_MON_SIZE]

def gen3_make_save(game_code, security_key_sec0, security_key_frlg, is_frlg):
    """Build a full 128KB Gen III save file."""
    buf = bytearray(GEN3_SIZE)

    name_bytes = gen3_encode("RED", 7)
    mon = gen3_make_party_mon(GEN3P_SPECIES, TEST_LEVEL, TEST_MOVE1,
                               TEST_TID, TEST_SID)

    sec0_data = gen3_make_section0(name_bytes, TEST_TID, TEST_SID,
                                    game_code, security_key_frlg)

    # Badges in section 0 at offset 0x0083 (SaveBlock2 badges field for Gen III)
    sec0_list = bytearray(sec0_data)
    sec0_list[0x0083] = TEST_BADGES

    if is_frlg:
        sk = security_key_frlg
        sec1_data = gen3_make_section1_frlg(TEST_MONEY, sk, mon)
    else:
        sk = security_key_sec0
        sec1_data = gen3_make_section1_rse(TEST_MONEY, sk, mon)

    # Write sections 0-13 (slot 0) and mirror as slot 1 (same data, count=2)
    sections_data = [bytearray(sec0_list), bytearray(sec1_data)]
    for i in range(2, GEN3_NUM_SECTIONS):
        sections_data.append(bytearray(GEN3_SECTION_SIZES.get(i, 3968)))

    # Populate box 1 slot 0 with Bulbasaur (national dex 1) for box-read tests.
    box_mon = gen3_make_box_mon(GEN3P_SPECIES, TEST_LEVEL, TEST_MOVE1,
                                TEST_TID, TEST_SID, pid=0x12345678)
    # Linear PC offset: 1 (currentBox byte) + box*30*80 + slot*80
    lin = 1 + 1 * GEN3_SLOTS_PER_BOX * GEN3_BOX_MON_SIZE + 0 * GEN3_BOX_MON_SIZE
    gen3_pc_write(sections_data, lin, box_mon)

    save_count = 1
    for i in range(GEN3_NUM_SECTIONS):
        sec = gen3_make_section(i, bytearray(sections_data[i]), save_count)
        buf[i * GEN3_SECTION_SIZE : (i+1) * GEN3_SECTION_SIZE] = sec

    # Slot 1 (sections 14-27): same data, higher save count so slot 0 wins
    # (lower save count = older save = we want slot 0 active)
    # Actually: HIGHER save count = more recent = active slot.
    # So slot 0 (count=1) is active if slot 1 has count=0.
    for i in range(GEN3_NUM_SECTIONS):
        sec = gen3_make_section(i, bytearray(sections_data[i]), 0)
        off = (GEN3_NUM_SECTIONS + i) * GEN3_SECTION_SIZE
        buf[off : off + GEN3_SECTION_SIZE] = sec

    return bytes(buf)

def make_gen3_rs():
    return gen3_make_save(game_code=0, security_key_sec0=0, security_key_frlg=0, is_frlg=False)

def make_gen3_emerald():
    # Emerald: security key at section 0 offset 0x00AC must be non-zero (distinguishes from RS=0).
    # Detection: RS=0, Emerald=non-zero & non-1, FRLG=1.
    EMERALD_KEY = 0x12345678
    return gen3_make_save(game_code=EMERALD_KEY, security_key_sec0=EMERALD_KEY, security_key_frlg=0, is_frlg=False)

def make_gen3_frlg():
    return gen3_make_save(game_code=1, security_key_sec0=0, security_key_frlg=0, is_frlg=True)

# ── Gen IV (D/P, Platinum, HG/SS) — 512 KB ───────────────────────────────────
# Two save slots, each 0x40000 bytes.
# Small block (general): starts at 0x00000 for slot 0 (trainer, party, bag)
# Large block (storage): follows small block (PC boxes)
# Footer (last 0x14 bytes of each block):
#   +0x00: block connection ID (4)
#   +0x04: save number (4)
#   +0x08: block size (4)  ← game detection via this field
#   +0x0C: K value (4) = 0x20060623
#   +0x10: T value (2)
#   +0x12: CRC-16-CCITT (2)
# DP general block size: 0xC100, Platinum: 0xCF2C, HGSS: 0xF700

GEN4_SIZE = 512 * 1024
GEN4_FOOTER_K = 0x20060623

GEN4_GENERAL_SIZES = {
    'dp':  0xC100,
    'pt':  0xCF2C,
    'hgss':0xF700,
}
GEN4_STORAGE_SIZES = {
    'dp':  0x121E0,
    'pt':  0x121E0,
    'hgss':0x12E00,
}

# Gen IV trainer data offsets in the general block:
# 0x0064: trainer name (16 bytes UTF-16LE)
# 0x0074: TID (2 LE), 0x0076: SID (2 LE)
# 0x0078: money (4 LE)
# 0x007C: gender (1)
# 0x007E: badges (1)
# Party at 0x0098 (DP): team count (4 bytes), then 6 × 236 bytes
# Actually from research: party starts at 0xA0 for DP/Pt

GEN4_PARTY_OFF = {
    'dp':  0x0098,
    'pt':  0x0098,
    'hgss':0x00DC,
}

def gen4_make_general_block(variant):
    size = GEN4_GENERAL_SIZES[variant]
    buf = bytearray(size)

    # Trainer name at 0x0064
    buf[0x0064:0x0074] = gen45_encode("RED", 16)
    struct.pack_into('<H', buf, 0x0074, TEST_TID)
    struct.pack_into('<H', buf, 0x0076, TEST_SID)
    struct.pack_into('<I', buf, 0x0078, TEST_MONEY)
    buf[0x007C] = 0   # gender male
    buf[0x007E] = TEST_BADGES

    # Party data
    party_off = GEN4_PARTY_OFF[variant]
    buf[party_off:party_off+4] = struct.pack('<I', 1)   # team count
    mon = gen4_make_party_mon(GEN3P_SPECIES, TEST_LEVEL, TEST_MOVE1, TEST_TID, TEST_SID)
    buf[party_off+4:party_off+4+236] = mon

    # Build footer
    footer_off = size - 0x14
    connection_id = 1
    save_num = 1
    struct.pack_into('<I', buf, footer_off + 0x00, connection_id)
    struct.pack_into('<I', buf, footer_off + 0x04, save_num)
    struct.pack_into('<I', buf, footer_off + 0x08, size)
    struct.pack_into('<I', buf, footer_off + 0x0C, GEN4_FOOTER_K)
    struct.pack_into('<H', buf, footer_off + 0x10, 0)  # T value

    crc = crc16_ccitt(bytes(buf[:footer_off]))
    struct.pack_into('<H', buf, footer_off + 0x12, crc)

    return bytes(buf)

GEN4_BOX_MON_SIZE = 136
GEN4_SLOTS_PER_BOX = 30
GEN4_NUM_BOXES = 18
GEN4_PC_BOX_BASE = 0x04

def gen4_make_box_mon(species, level, move1, ot_tid, ot_sid, pid=0):
    """First 136 bytes of a Gen IV party mon = the box mon.

    Uses pid=0 so the Growth (species/item) substruct lands at the start
    of the encrypted region — the same workaround the party fixture relies
    on for its nickname/OT writes that happen to overlap encrypted bytes.
    """
    return gen4_make_party_mon(species, level, move1, ot_tid, ot_sid, pid)[:GEN4_BOX_MON_SIZE]

def gen4_make_storage_block(variant):
    size = GEN4_STORAGE_SIZES[variant]
    buf = bytearray(size)
    # Populate box 1 slot 0 with Bulbasaur (national dex 1) so gen4_get_box
    # has something concrete to read in tests.
    box_mon = gen4_make_box_mon(GEN3P_SPECIES, TEST_LEVEL, TEST_MOVE1,
                                TEST_TID, TEST_SID)
    off = GEN4_PC_BOX_BASE + (1 * GEN4_SLOTS_PER_BOX + 0) * GEN4_BOX_MON_SIZE
    buf[off:off+GEN4_BOX_MON_SIZE] = box_mon

    footer_off = size - 0x14
    connection_id = 1
    save_num = 1
    struct.pack_into('<I', buf, footer_off + 0x00, connection_id)
    struct.pack_into('<I', buf, footer_off + 0x04, save_num)
    struct.pack_into('<I', buf, footer_off + 0x08, size)
    struct.pack_into('<I', buf, footer_off + 0x0C, GEN4_FOOTER_K)
    crc = crc16_ccitt(bytes(buf[:footer_off]))
    struct.pack_into('<H', buf, footer_off + 0x12, crc)
    return bytes(buf)

def make_gen4(variant):
    buf = bytearray(GEN4_SIZE)
    general = gen4_make_general_block(variant)
    storage = gen4_make_storage_block(variant)
    # Slot 0: general at 0, storage after general
    buf[:len(general)] = general
    buf[len(general):len(general)+len(storage)] = storage
    return bytes(buf)

def make_gen4_dp():       return make_gen4('dp')
def make_gen4_platinum(): return make_gen4('pt')
def make_gen4_hgss():     return make_gen4('hgss')

# ── Gen V (Black/White, Black 2/White 2) — 512 KB ────────────────────────────
# Similar to Gen IV. Two general blocks (each 0x40000 bytes).
# General block sizes: BW ~0x26A00, BW2 ~0x26E00 (approximate from PKHeX)
# Footer: same structure as Gen IV.
# Trainer data: BW at 0x19D0, BW2 at 0x19D0 (approximate)

GEN5_SIZE = 512 * 1024
GEN5_GENERAL_SIZES = {
    'bw':  0x26A00,
    'bw2': 0x26E00,
}
GEN5_STORAGE_SIZE = 0x26000   # approximate

# Gen V trainer offsets (BW/BW2, offsets within general block)
GEN5_TRAINER_OFF = 0x19D0   # approximate — name(16), TID(2), SID(2), money(4)

GEN5_PC_BOX_BASE = 0x2000
GEN5_NUM_BOXES = 24
GEN5_SLOTS_PER_BOX = 30
GEN5_BOX_MON_SIZE = 136

def gen5_make_box_mon(species, level, move1, ot_tid, ot_sid, pid=0):
    return gen4_make_party_mon(species, level, move1, ot_tid, ot_sid, pid)[:GEN5_BOX_MON_SIZE]

def gen5_make_general_block(variant):
    size = GEN5_GENERAL_SIZES[variant]
    buf = bytearray(size)

    off = GEN5_TRAINER_OFF
    buf[off:off+16] = gen45_encode("RED", 16)
    struct.pack_into('<H', buf, off+16, TEST_TID)
    struct.pack_into('<H', buf, off+18, TEST_SID)
    struct.pack_into('<I', buf, off+20, TEST_MONEY)
    buf[off+24] = TEST_BADGES

    # Party offset in Gen V: approximately 0x200 from block start for BW
    party_off = 0x0200
    buf[party_off:party_off+4] = struct.pack('<I', 1)   # 1 Pokemon in party
    mon = gen4_make_party_mon(GEN3P_SPECIES, TEST_LEVEL, TEST_MOVE1, TEST_TID, TEST_SID)
    buf[party_off+4:party_off+4+236] = mon

    # PC box 1 slot 0 = Bulbasaur, for gen5_get_box tests
    box_mon = gen5_make_box_mon(GEN3P_SPECIES, TEST_LEVEL, TEST_MOVE1, TEST_TID, TEST_SID)
    box_off = GEN5_PC_BOX_BASE + (1 * GEN5_SLOTS_PER_BOX + 0) * GEN5_BOX_MON_SIZE
    buf[box_off:box_off+GEN5_BOX_MON_SIZE] = box_mon

    footer_off = size - 0x14
    connection_id = 1
    save_num = 1
    struct.pack_into('<I', buf, footer_off + 0x00, connection_id)
    struct.pack_into('<I', buf, footer_off + 0x04, save_num)
    struct.pack_into('<I', buf, footer_off + 0x08, size)
    struct.pack_into('<I', buf, footer_off + 0x0C, GEN4_FOOTER_K)
    crc = crc16_ccitt(bytes(buf[:footer_off]))
    struct.pack_into('<H', buf, footer_off + 0x12, crc)
    return bytes(buf)

def make_gen5(variant):
    buf = bytearray(GEN5_SIZE)
    general = gen5_make_general_block(variant)
    buf[:len(general)] = general
    return bytes(buf)

def make_gen5_bw():  return make_gen5('bw')
def make_gen5_bw2(): return make_gen5('bw2')

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
