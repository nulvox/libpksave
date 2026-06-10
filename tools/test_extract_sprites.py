#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Unit tests for tools/extract_sprites.py.

Standalone script — no pytest. Run from anywhere:

    python3 tools/test_extract_sprites.py

Exits 0 on success, 1 on the first failed assertion.
"""

import os
import struct
import sys
import tempfile
import traceback

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import extract_sprites as ex  # noqa: E402


def _header_with_game_code(code: bytes) -> bytes:
    """Return a 0xB0-byte buffer with `code` (4 bytes ASCII) at offset 0xAC."""
    assert len(code) == 4
    buf = bytearray(0xB0)
    buf[0xAC:0xB0] = code
    return bytes(buf)


def test_detect_ruby_us():
    rom = _header_with_game_code(b"AXVE")
    assert ex.detect_rom_variant(rom) == "RS", \
        "AXVE (Ruby US) should detect as RS"


def test_detect_sapphire_us():
    rom = _header_with_game_code(b"AXPE")
    assert ex.detect_rom_variant(rom) == "RS", \
        "AXPE (Sapphire US) shares the RS sprite table"


def test_detect_emerald_us():
    rom = _header_with_game_code(b"BPEE")
    assert ex.detect_rom_variant(rom) == "EMERALD"


def test_detect_firered_us():
    rom = _header_with_game_code(b"BPRE")
    assert ex.detect_rom_variant(rom) == "FRLG"


def test_detect_leafgreen_us():
    rom = _header_with_game_code(b"BPGE")
    assert ex.detect_rom_variant(rom) == "FRLG", \
        "BPGE (LeafGreen US) shares the FRLG sprite table"


def test_detect_unknown_code():
    rom = _header_with_game_code(b"ZZZZ")
    assert ex.detect_rom_variant(rom) == "UNKNOWN"


def test_detect_buffer_too_short():
    assert ex.detect_rom_variant(b"") == "UNKNOWN"
    assert ex.detect_rom_variant(b"\x00" * 0xAF) == "UNKNOWN"


# ── gba_pointer ──────────────────────────────────────────────────────────────

def test_pointer_strips_rom_base():
    # 0x081ECBB4 little-endian = B4 CB 1E 08 — canonical Ruby gMonFrontPicTable
    rom = b"\xB4\xCB\x1E\x08"
    assert ex.gba_pointer(rom, 0) == 0x001ECBB4


def test_pointer_at_arbitrary_offset():
    rom = b"\xFF\xFF" + b"\x00\x00\x00\x08" + b"\xFF"
    assert ex.gba_pointer(rom, 2) == 0x00000000


def test_pointer_rejects_non_rom_target():
    # 0x02000000 is EWRAM, not cartridge ROM — must reject.
    rom = b"\x00\x00\x00\x02"
    try:
        ex.gba_pointer(rom, 0)
    except ValueError:
        return
    raise AssertionError("non-ROM pointer should raise ValueError")


def test_pointer_buffer_too_short():
    try:
        ex.gba_pointer(b"\x00\x00", 0)
    except (ValueError, IndexError):
        return
    raise AssertionError("short buffer should raise")


# ── lz77_decompress ──────────────────────────────────────────────────────────

def test_lz77_run_of_one_byte():
    # GBA BIOS LZ10 — encode 5 'A's:
    #   header  : 0x10, then 3-byte LE uncompressed size = 5
    #   flag    : 0b01000000 = literal, back-ref, ...rest unused
    #   token 1 : literal 'A' (0x41)
    #   token 2 : back-ref length=4 (N=1, encoded high-nibble of byte0),
    #             offset-1=0 → byte0=0x10, byte1=0x00 → repeat last byte 4×
    data = bytes([0x10, 0x05, 0x00, 0x00,
                  0x40, 0x41, 0x10, 0x00])
    assert ex.lz77_decompress(data, 0) == b"AAAAA"


def test_lz77_all_literals():
    # 4 literal bytes "WXYZ"; flag = 0b00000000 = all literal
    data = bytes([0x10, 0x04, 0x00, 0x00,
                  0x00, ord("W"), ord("X"), ord("Y"), ord("Z")])
    assert ex.lz77_decompress(data, 0) == b"WXYZ"


def test_lz77_offset_in_stream():
    # Same payload as test_lz77_run_of_one_byte but offset by a junk prefix.
    junk = b"\xDE\xAD\xBE\xEF"
    data = junk + bytes([0x10, 0x05, 0x00, 0x00,
                         0x40, 0x41, 0x10, 0x00])
    assert ex.lz77_decompress(data, len(junk)) == b"AAAAA"


def test_lz77_rejects_bad_header():
    # First byte must be 0x10 for LZ10. Reject anything else.
    data = bytes([0x11, 0x05, 0x00, 0x00, 0x00])
    try:
        ex.lz77_decompress(data, 0)
    except ValueError:
        return
    raise AssertionError("non-LZ10 header should raise ValueError")


# ── untile_4bpp ──────────────────────────────────────────────────────────────

def test_untile_4bpp_unpacks_nibbles():
    # Single 8x8 tile, all bytes = 0x21:
    #   each byte holds two pixels, low nibble first → pixel[0]=1, pixel[1]=2
    # Repeated for the whole tile: every row should be [1,2,1,2,1,2,1,2].
    tile = bytes([0x21]) * 32
    out = ex.untile_4bpp(tile, 1, 1)
    assert len(out) == 64, f"1x1 tile → 8x8 = 64 pixels, got {len(out)}"
    for row in range(8):
        for col in range(8):
            expected = 1 if (col % 2 == 0) else 2
            assert out[row * 8 + col] == expected, \
                f"row={row} col={col}: expected {expected}, got {out[row*8+col]}"


def test_untile_4bpp_tile_ordering_is_row_major():
    # 2x1 tile grid. Tile 0 = all 0x11 (pal index 1 everywhere).
    # Tile 1 = all 0x22 (pal index 2 everywhere). Output is 16x8 px.
    # Row-major tile stream means cols 0..7 are tile-0 (idx 1), 8..15 tile-1 (idx 2).
    tile0 = bytes([0x11]) * 32
    tile1 = bytes([0x22]) * 32
    out = ex.untile_4bpp(tile0 + tile1, 2, 1)
    assert len(out) == 16 * 8
    for row in range(8):
        for col in range(16):
            expected = 1 if col < 8 else 2
            assert out[row * 16 + col] == expected, \
                f"row={row} col={col}: expected {expected}"


def test_untile_4bpp_8x8_tile_full_sprite_dims():
    # 8x8 tile grid (a Pokemon 64x64 sprite) yields exactly 4096 pixels.
    sprite = bytes(32 * 64)  # all zero
    out = ex.untile_4bpp(sprite, 8, 8)
    assert len(out) == 64 * 64
    assert all(p == 0 for p in out)


# ── write_indexed_bmp_4bpp ───────────────────────────────────────────────────

def _parse_indexed_bmp(path):
    """Minimal BMP parser for the 4bpp shape our writer emits.

    Returns (width, height, palette_rgb, pixels) where pixels are top-down
    row-major palette indices. Validates butano grit's BMP requirements:
    BITMAPINFOHEADER (size=40), 4bpp, BI_RGB compression, multiple-of-8
    width/height.
    """
    with open(path, "rb") as f:
        data = f.read()
    assert data[:2] == b"BM", f"bad signature: {data[:2]!r}"
    _file_size, _r1, _r2, pixel_off = struct.unpack("<IHHI", data[2:14])
    (dib_size,) = struct.unpack("<I", data[14:18])
    assert dib_size == 40, f"expected BITMAPINFOHEADER (40), got {dib_size}"
    w, h, planes, bpp, compression, image_size, _xppm, _yppm, clr_used, _clr_imp = \
        struct.unpack("<iiHHIIiiII", data[18:54])
    assert planes == 1 and bpp == 4 and compression == 0, \
        f"expected planes=1 bpp=4 compression=0; got planes={planes} bpp={bpp} compression={compression}"
    assert w % 8 == 0 and h % 8 == 0, f"butano requires w/h %8==0; got {w}x{h}"
    # Palette: 16 entries × 4 bytes BGRA → list of (R, G, B) tuples
    palette = []
    pal_off = 14 + dib_size
    for i in range(16):
        b, g, r, _a = data[pal_off + i * 4:pal_off + i * 4 + 4]
        palette.append((r, g, b))
    # Pixels: bottom-up rows, high-nibble = left pixel. Unpack to top-down.
    row_bytes = w // 2
    pixels = bytearray(w * h)
    for img_row in range(h):
        bmp_row = h - 1 - img_row
        src = pixel_off + bmp_row * row_bytes
        for col in range(0, w, 2):
            byte = data[src + col // 2]
            pixels[img_row * w + col]     = (byte >> 4) & 0x0F
            pixels[img_row * w + col + 1] = byte & 0x0F
    return w, h, palette, bytes(pixels)


def test_bmp_smoke_dims_palette_pixels():
    palette = [(0, 0, 0), (255, 0, 0)] + [(0, 0, 0)] * 14
    # 8x8 (smallest butano-acceptable size). Diagonal stripe of pal idx 1.
    pixels = bytearray(64)
    for d in range(8):
        pixels[d * 8 + d] = 1
    with tempfile.TemporaryDirectory() as td:
        path = os.path.join(td, "smoke.bmp")
        ex.write_indexed_bmp_4bpp(path, 8, 8, bytes(pixels), palette)
        w, h, pal, px = _parse_indexed_bmp(path)
    assert (w, h) == (8, 8)
    assert pal[1] == (255, 0, 0)
    assert px == bytes(pixels)


def test_bmp_roundtrip_64x64_full_palette():
    palette = [(i * 16, i * 16, i * 16) for i in range(16)]
    pixels = bytes((row % 16) for row in range(64) for _col in range(64))
    with tempfile.TemporaryDirectory() as td:
        path = os.path.join(td, "sprite.bmp")
        ex.write_indexed_bmp_4bpp(path, 64, 64, pixels, palette)
        w, h, pal, px = _parse_indexed_bmp(path)
    assert (w, h) == (64, 64)
    assert pal == palette
    assert px == pixels


def test_bmp_rejects_non_aligned_dims():
    try:
        ex.write_indexed_bmp_4bpp("/dev/null", 7, 8, b"\x00" * 56, [(0, 0, 0)] * 16)
    except ValueError:
        return
    raise AssertionError("width not multiple of 8 should raise")


# ── bgr555_to_rgb ────────────────────────────────────────────────────────────

def test_bgr555_black_and_white():
    # Entry 0 = 0x0000 (black). Entry 1 = 0x7FFF (max R, G, B all 0x1F).
    raw = bytes([0x00, 0x00, 0xFF, 0x7F] + [0] * 28)
    pal = ex.bgr555_to_rgb(raw)
    assert pal[0] == (0, 0, 0)
    # 0x1F << 3 = 0xF8 (top 5 of 8 bits set; low 3 bits zero)
    assert pal[1] == (0xF8, 0xF8, 0xF8)


def test_bgr555_channel_isolation():
    # R only: 0x001F = R=0x1F, G=0, B=0
    raw = bytes([0x1F, 0x00] + [0] * 30)
    assert ex.bgr555_to_rgb(raw)[0] == (0xF8, 0, 0)
    # G only: 0x03E0 = R=0, G=0x1F, B=0
    raw = bytes([0xE0, 0x03] + [0] * 30)
    assert ex.bgr555_to_rgb(raw)[0] == (0, 0xF8, 0)
    # B only: 0x7C00 = R=0, G=0, B=0x1F
    raw = bytes([0x00, 0x7C] + [0] * 30)
    assert ex.bgr555_to_rgb(raw)[0] == (0, 0, 0xF8)


def test_bgr555_length_check():
    try:
        ex.bgr555_to_rgb(b"\x00" * 31)
    except (AssertionError, ValueError):
        return
    raise AssertionError("31-byte palette should be rejected")


# ── extract_species ──────────────────────────────────────────────────────────

def _lz10_all_literal(data: bytes) -> bytes:
    """Encode `data` as an LZ10 stream using only literal tokens.

    Real ROMs use compression, but the decompressor is format-agnostic
    on the flag byte — all-literal is valid LZ10 and exercises the same
    code path while staying trivial to construct from the test side.
    """
    out = bytearray([0x10, len(data) & 0xFF,
                     (len(data) >> 8) & 0xFF, (len(data) >> 16) & 0xFF])
    for i in range(0, len(data), 8):
        chunk = data[i:i + 8]
        out.append(0x00)  # flag: all-literal
        out.extend(chunk)
    return bytes(out)


def _le_u32(value: int) -> bytes:
    return value.to_bytes(4, "little")


def _le_u16(value: int) -> bytes:
    return value.to_bytes(2, "little")


def test_extract_species_returns_pixels_and_palette():
    # Build a synthetic ROM with one sprite slot:
    #   sprite_raw    : 2048 bytes uncompressed (64×64 px, all palette index 0)
    #   palette_raw   : 32 bytes uncompressed (16 entries × 2 bytes BGR555 = 0)
    #   pic_entry     : (ptr_to_sprite, size=2048, tag=0)
    #   pal_entry     : (ptr_to_palette, tag=0, padding=0)
    sprite_raw  = bytes(2048)
    palette_raw = bytes(32)
    sprite_lz   = _lz10_all_literal(sprite_raw)
    palette_lz  = _lz10_all_literal(palette_raw)

    rom = bytearray(0xB0)
    rom[0xAC:0xB0] = b"AXVE"
    sprite_off = len(rom);  rom.extend(sprite_lz)
    palette_off = len(rom); rom.extend(palette_lz)
    pic_entry_off = len(rom)
    rom.extend(_le_u32(0x08000000 + sprite_off) + _le_u16(2048) + _le_u16(0))
    pal_entry_off = len(rom)
    rom.extend(_le_u32(0x08000000 + palette_off) + _le_u16(0) + _le_u16(0))

    pixels, palette = ex.extract_species(bytes(rom), pic_entry_off, pal_entry_off)
    assert len(pixels) == 64 * 64, f"expected 4096-byte pixel buffer, got {len(pixels)}"
    assert all(p == 0 for p in pixels), "all-zero sprite should decode to all-index-0 pixels"
    assert len(palette) == 16, f"expected 16-entry palette, got {len(palette)}"
    assert palette[0] == (0, 0, 0), "all-zero palette entry should decode to black"


def test_extract_species_round_trips_nontrivial_pixels():
    # Sprite: every byte = 0x32 → pixel pattern [2, 3, 2, 3, ...].
    # Palette: 16 entries each at 0x7C00 << shift to make them distinguishable.
    sprite_raw = bytes([0x32]) * 2048
    palette_raw = bytearray(32)
    # entry 2 = 0x7C00 (pure blue), entry 3 = 0x03E0 (pure green)
    palette_raw[4:6] = [0x00, 0x7C]
    palette_raw[6:8] = [0xE0, 0x03]

    sprite_lz  = _lz10_all_literal(sprite_raw)
    palette_lz = _lz10_all_literal(bytes(palette_raw))

    rom = bytearray(0xB0)
    rom[0xAC:0xB0] = b"AXVE"
    sprite_off  = len(rom); rom.extend(sprite_lz)
    palette_off = len(rom); rom.extend(palette_lz)
    pic_entry_off = len(rom)
    rom.extend(_le_u32(0x08000000 + sprite_off) + _le_u16(2048) + _le_u16(0))
    pal_entry_off = len(rom)
    rom.extend(_le_u32(0x08000000 + palette_off) + _le_u16(0) + _le_u16(0))

    pixels, palette = ex.extract_species(bytes(rom), pic_entry_off, pal_entry_off)
    assert pixels[0] == 2 and pixels[1] == 3, \
        f"expected [2,3,...], got [{pixels[0]},{pixels[1]},...]"
    assert palette[2] == (0, 0, 0xF8), "palette index 2 should be pure blue"
    assert palette[3] == (0, 0xF8, 0), "palette index 3 should be pure green"


# ── extract_all: orchestration + idempotency ─────────────────────────────────

def _synthetic_rom_with_species(species_count: int, *, game_code=b"AXVE"):
    """Build a tiny ROM containing `species_count` species. Returns
    (rom_bytes, pic_table_off, pal_table_off)."""
    rom = bytearray(0xB0)
    rom[0xAC:0xB0] = game_code
    # Per-species sprite + palette blobs, ascending fill so each PNG differs.
    sprite_offs = []
    palette_offs = []
    for s in range(species_count):
        sprite = bytes([(s & 0x0F) | ((s & 0x0F) << 4)]) * 2048
        palette = bytearray(32)
        # set entry s with a recognizable BGR555 value
        palette[2 * s:2 * s + 2] = [0x00, 0x7C]  # pure blue
        sprite_offs.append(len(rom))
        rom.extend(_lz10_all_literal(sprite))
        palette_offs.append(len(rom))
        rom.extend(_lz10_all_literal(bytes(palette)))
    pic_table_off = len(rom)
    for so in sprite_offs:
        rom.extend(_le_u32(0x08000000 + so) + _le_u16(2048) + _le_u16(0))
    pal_table_off = len(rom)
    for po in palette_offs:
        rom.extend(_le_u32(0x08000000 + po) + _le_u16(0) + _le_u16(0))
    return bytes(rom), pic_table_off, pal_table_off


def test_extract_all_writes_one_bmp_per_species():
    rom, pic_off, pal_off = _synthetic_rom_with_species(3)
    with tempfile.TemporaryDirectory() as td:
        rom_path = os.path.join(td, "synth.gba")
        out_dir = os.path.join(td, "out")
        with open(rom_path, "wb") as f:
            f.write(rom)
        n = ex.extract_all(rom_path, out_dir,
                           _table_override=(pic_off, pal_off, 3))
        assert n == 3
        for s in range(3):
            bmp_path = os.path.join(out_dir, f"species_{s + 1:03d}.bmp")
            assert os.path.exists(bmp_path), f"missing {bmp_path}"
            w, h, _pal, _px = _parse_indexed_bmp(bmp_path)
            assert (w, h) == (64, 64)


def test_extract_all_is_idempotent_when_dir_populated():
    rom, pic_off, pal_off = _synthetic_rom_with_species(2)
    with tempfile.TemporaryDirectory() as td:
        rom_path = os.path.join(td, "synth.gba")
        out_dir = os.path.join(td, "out")
        os.makedirs(out_dir, exist_ok=True)
        # Pre-populate with a sentinel; extract_all should leave it alone.
        marker = os.path.join(out_dir, "species_001.bmp")
        with open(marker, "wb") as f:
            f.write(b"PRE-EXISTING")
        with open(rom_path, "wb") as f:
            f.write(rom)
        n = ex.extract_all(rom_path, out_dir,
                           _table_override=(pic_off, pal_off, 2))
        assert n == 0, f"expected 0 extractions on populated dir, got {n}"
        with open(marker, "rb") as f:
            assert f.read() == b"PRE-EXISTING", "sentinel was overwritten"


def test_extract_all_rejects_unknown_variant_without_override():
    # No game code → UNKNOWN → no canonical table → must raise.
    rom = bytearray(0xB0)
    rom[0xAC:0xB0] = b"ZZZZ"
    with tempfile.TemporaryDirectory() as td:
        rom_path = os.path.join(td, "junk.gba")
        with open(rom_path, "wb") as f:
            f.write(rom)
        try:
            ex.extract_all(rom_path, os.path.join(td, "out"))
        except ValueError:
            return
        raise AssertionError("UNKNOWN variant should raise without override")


# ── runner ───────────────────────────────────────────────────────────────────

def main() -> int:
    tests = [v for k, v in globals().items() if k.startswith("test_")]
    failures = []
    for t in tests:
        try:
            t()
            print(f"  PASS  {t.__name__}")
        except Exception:
            print(f"  FAIL  {t.__name__}")
            traceback.print_exc()
            failures.append(t.__name__)
    if failures:
        print(f"\n=== {len(failures)} test(s) FAILED ===")
        return 1
    print(f"\n=== {len(tests)} test(s) PASSED ===")
    return 0


if __name__ == "__main__":
    sys.exit(main())
