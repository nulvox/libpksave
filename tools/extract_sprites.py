#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Extract Pokemon front sprites from a user-supplied Gen III GBA ROM.

Reads the game code at the GBA header (offset 0xAC..0xB0) to identify the
ROM variant, then walks gMonFrontPicTable to dump every species sprite as
a 64x64 indexed PNG into the output directory. No-op when the output
directory is already populated.

Usage:
    python3 tools/extract_sprites.py --rom /path/to/ruby.gba \\
        --out frontend-gba/assets/sprites/

Legal: the repository ships only a generic silhouette. Real sprite art
is Game Freak / Nintendo property; this tool only operates on a ROM the
user already legally owns and never bundles its output into the
distributed binary.

Data references: pret/pokeruby and pret/pokeemerald species_info /
gMonFrontPicTable sources.
"""

# Variant name mirrors libpksave's pksave_game_t naming (RS / EMERALD /
# FRLG). Ruby and Sapphire share sprite tables; same for FireRed/LeafGreen.
_GAME_CODE_TO_VARIANT = {
    b"AXVE": "RS",        # Ruby US
    b"AXPE": "RS",        # Sapphire US
    b"BPEE": "EMERALD",   # Emerald US
    b"BPRE": "FRLG",      # FireRed US
    b"BPGE": "FRLG",      # LeafGreen US
}


import argparse
import os
import struct
import sys

_ROM_BASE_MASK = 0xFE000000  # 0x08xxxxxx or 0x09xxxxxx — cartridge ROM
_ROM_BASE      = 0x08000000

def write_indexed_bmp_4bpp(path: str, width: int, height: int,
                           pixels: bytes, palette) -> None:
    """Write a Windows-standard 4bpp indexed BMP consumable by butano's grit.

    `palette` is a 16-entry list of (R, G, B) tuples. `pixels` is a flat
    row-major buffer of palette indices (0..15), length == width * height.

    Output conforms to BITMAPINFOHEADER (40-byte DIB) — butano's bmp.py
    rejects v4/v5 headers. Rows are stored bottom-up, two pixels packed
    per byte with the high nibble holding the LEFT pixel (standard BMP
    nibble order; opposite of the GBA tile convention, so we re-pack).
    """
    if width % 8 != 0 or height % 8 != 0:
        raise ValueError(f"width/height must be multiples of 8 ({width}x{height})")
    if len(pixels) != width * height:
        raise ValueError(f"pixel buffer {len(pixels)} != {width}*{height}")
    if len(palette) != 16:
        raise ValueError(f"4bpp BMP needs exactly 16 palette entries, got {len(palette)}")

    row_bytes = width // 2  # 4bpp: each row width/2 bytes. width%8==0 → already 4-byte aligned.
    pixel_data = bytearray(row_bytes * height)
    # Rows bottom-up: BMP row 0 = image row (height-1), nibble high = left pixel.
    for img_row in range(height):
        bmp_row = (height - 1 - img_row)
        src_off = img_row * width
        dst_off = bmp_row * row_bytes
        for col in range(0, width, 2):
            left  = pixels[src_off + col]     & 0x0F
            right = pixels[src_off + col + 1] & 0x0F
            pixel_data[dst_off + col // 2] = (left << 4) | right

    file_header_size = 14
    dib_header_size  = 40
    palette_size     = 16 * 4
    pixel_offset     = file_header_size + dib_header_size + palette_size
    total_size       = pixel_offset + len(pixel_data)

    file_header = b"BM" + struct.pack("<IHHI", total_size, 0, 0, pixel_offset)
    dib_header  = struct.pack("<IiiHHIIiiII",
        dib_header_size, width, height, 1, 4, 0, len(pixel_data),
        0, 0, 16, 0)
    palette_bytes = bytearray()
    for r, g, b in palette:
        palette_bytes.extend((b & 0xFF, g & 0xFF, r & 0xFF, 0))

    with open(path, "wb") as f:
        f.write(file_header)
        f.write(dib_header)
        f.write(bytes(palette_bytes))
        f.write(bytes(pixel_data))


def gba_pointer(rom_bytes: bytes, addr: int) -> int:
    """Read a 32-bit little-endian pointer at `addr`, return ROM file offset.

    The on-cart pointer holds a mapped address (0x08xxxxxx or 0x09xxxxxx);
    the returned offset has the cartridge base stripped so it indexes
    directly into `rom_bytes`. Raises ValueError if the pointer doesn't
    target cartridge ROM or the buffer is too short.
    """
    if addr < 0 or addr + 4 > len(rom_bytes):
        raise ValueError(
            f"pointer read at {addr:#x} exceeds buffer size {len(rom_bytes):#x}"
        )
    raw = int.from_bytes(rom_bytes[addr:addr + 4], "little")
    if (raw & _ROM_BASE_MASK) != _ROM_BASE:
        raise ValueError(
            f"pointer {raw:#010x} at {addr:#x} does not target cartridge ROM"
        )
    return raw - _ROM_BASE


def lz77_decompress(data: bytes, offset: int) -> bytes:
    """Decompress a GBA BIOS LZ77 (LZ10) blob starting at `offset`.

    Header (4 bytes): 0x10 then 3-byte little-endian uncompressed size.
    Body: alternating 1 flag byte (MSB-first, 1=back-ref / 0=literal) and
    8 tokens. A back-ref is two bytes — high nibble of byte0 = (length-3),
    remaining 12 bits = (offset-1); offset is measured backwards from the
    current decompressed length.
    """
    if offset + 4 > len(data) or data[offset] != 0x10:
        raise ValueError(f"not an LZ10 stream at offset {offset:#x}")
    uncompressed_size = data[offset + 1] | (data[offset + 2] << 8) | (data[offset + 3] << 16)
    out = bytearray()
    pos = offset + 4
    while len(out) < uncompressed_size:
        flags = data[pos]
        pos += 1
        for bit in range(8):
            if len(out) >= uncompressed_size:
                break
            if flags & (0x80 >> bit):
                b0 = data[pos]
                b1 = data[pos + 1]
                pos += 2
                length = (b0 >> 4) + 3
                back   = (((b0 & 0x0F) << 8) | b1) + 1
                src = len(out) - back
                for i in range(length):
                    out.append(out[src + i])
            else:
                out.append(data[pos])
                pos += 1
    return bytes(out)


# Gen 3 sprite layout: 64×64 px = 8×8 tile grid; each tile 32 bytes 4bpp.
_SPRITE_W_TILES = 8
_SPRITE_H_TILES = 8
_SPRITE_PIXEL_COUNT = 64 * 64

# pic table entry: u32 data_ptr, u16 size, u16 tag — 8 bytes total.
# pal table entry: u32 data_ptr, u16 tag, u16 pad  — 8 bytes total.
_PIC_ENTRY_SIZE = 8
_PAL_ENTRY_SIZE = 8


# Canonical per-variant table addresses sourced from pret/pokeruby,
# pret/pokeemerald, pret/pokefirered (gMonFrontPicTable / gMonPaletteTable
# symbols). Populate from a pret checkout before running against real ROMs;
# see docs/frontend-gba-plan.md §"Checkpoint #6" follow-ups.
#
# Schema: variant → (pic_table_file_off, pal_table_file_off, n_species)
_VARIANT_TABLES = {
    # "RS":      (0x001ECBB4, 0x001EC284, 412),   # TODO: verify against pret
    # "EMERALD": (0x0030A18C, 0x0030856C, 412),
    # "FRLG":    (0x00234C18, 0x002362F4, 412),
}


def _bmp_name_for_species(species_index_zero_based: int) -> str:
    """Output naming: 001.bmp .. 387.bmp, one-based to match pret's species enum."""
    return f"species_{species_index_zero_based + 1:03d}.bmp"


def extract_all(rom_path: str, out_dir: str, *, _table_override=None) -> int:
    """Walk a Gen 3 ROM's species table and write one PNG per species.

    No-op when `out_dir` already contains any PNGs (idempotency).
    Returns the number of PNGs written.

    Raises ValueError on UNKNOWN variants unless `_table_override` is
    supplied (test injection: (pic_table_off, pal_table_off, n_species)).
    """
    with open(rom_path, "rb") as f:
        rom = f.read()

    if _table_override is not None:
        pic_table_off, pal_table_off, n_species = _table_override
    else:
        variant = detect_rom_variant(rom)
        if variant not in _VARIANT_TABLES:
            raise ValueError(
                f"no canonical sprite-table addresses registered for variant "
                f"{variant!r}. Populate _VARIANT_TABLES in tools/extract_sprites.py "
                f"from a pret checkout before extracting from real ROMs."
            )
        pic_table_off, pal_table_off, n_species = _VARIANT_TABLES[variant]

    os.makedirs(out_dir, exist_ok=True)
    if any(name.endswith(".bmp") for name in os.listdir(out_dir)):
        return 0

    written = 0
    for s in range(n_species):
        pic_entry_off = pic_table_off + s * _PIC_ENTRY_SIZE
        pal_entry_off = pal_table_off + s * _PAL_ENTRY_SIZE
        try:
            pixels, palette = extract_species(rom, pic_entry_off, pal_entry_off)
        except ValueError as exc:
            print(f"  skip species {s + 1}: {exc}", file=sys.stderr)
            continue
        bmp_name = _bmp_name_for_species(s)
        out_path = os.path.join(out_dir, bmp_name)
        write_indexed_bmp_4bpp(out_path, 64, 64, pixels, palette)
        # butano needs a JSON sidecar per BMP marking it as a sprite.
        json_path = os.path.join(out_dir, bmp_name[:-4] + ".json")
        with open(json_path, "w", encoding="ascii") as f:
            f.write('{"type": "sprite", "height": 64}\n')
        written += 1
    return written


def extract_species(rom_bytes: bytes, pic_entry_off: int, pal_entry_off: int):
    """Decode one species from a Gen 3 ROM.

    Returns (pixels, palette) where pixels is a 4096-byte row-major
    palette-index buffer (64×64) and palette is a 16-entry list of
    (R, G, B) tuples. The two `*_entry_off` arguments are file offsets
    of one entry in gMonFrontPicTable / gMonPaletteTable respectively.
    """
    sprite_data_off  = gba_pointer(rom_bytes, pic_entry_off)
    palette_data_off = gba_pointer(rom_bytes, pal_entry_off)
    sprite_tiles = lz77_decompress(rom_bytes, sprite_data_off)
    palette_raw  = lz77_decompress(rom_bytes, palette_data_off)
    pixels  = untile_4bpp(sprite_tiles, _SPRITE_W_TILES, _SPRITE_H_TILES)
    palette = bgr555_to_rgb(palette_raw)
    return pixels, palette


def bgr555_to_rgb(palette_bytes: bytes) -> list:
    """Convert 32 bytes of GBA BGR-555 (16 entries × 2 bytes LE) to RGB888.

    Each 16-bit entry packs B/G/R at 5 bits each (bit 15 unused). We
    upshift each channel by 3 — same as Pillow's `pal // 32 * 255 // 31`
    truncation; not bit-replicated because the silhouette + render path
    doesn't need that last bit of precision.
    """
    if len(palette_bytes) != 32:
        raise ValueError(f"expected 32-byte palette, got {len(palette_bytes)}")
    out = []
    for i in range(16):
        entry = palette_bytes[i * 2] | (palette_bytes[i * 2 + 1] << 8)
        r = (entry        & 0x1F) << 3
        g = ((entry >> 5)  & 0x1F) << 3
        b = ((entry >> 10) & 0x1F) << 3
        out.append((r, g, b))
    return out


def untile_4bpp(tile_data: bytes, w_tiles: int, h_tiles: int) -> bytes:
    """Unpack 8x8 4bpp tiles into a linear row-major palette-index grid.

    Each tile is 32 bytes (8 rows of 4 bytes; each byte = 2 pixels, low
    nibble = left). Tiles are arranged in row-major order in `tile_data`,
    which is the GBA's standard background/sprite layout after a BIOS
    LZ77 decompress of a Gen 3 front-pic blob.
    """
    width_px = w_tiles * 8
    height_px = h_tiles * 8
    pixels = bytearray(width_px * height_px)
    for tile_y in range(h_tiles):
        for tile_x in range(w_tiles):
            tile_offset = (tile_y * w_tiles + tile_x) * 32
            for row in range(8):
                row_offset = tile_offset + row * 4
                for byte_col in range(4):
                    byte = tile_data[row_offset + byte_col]
                    px_y = tile_y * 8 + row
                    px_x = tile_x * 8 + byte_col * 2
                    pixels[px_y * width_px + px_x]     = byte & 0x0F
                    pixels[px_y * width_px + px_x + 1] = (byte >> 4) & 0x0F
    return bytes(pixels)


def detect_rom_variant(rom_bytes: bytes) -> str:
    """Identify the ROM by its 4-byte game code at header offset 0xAC.

    Returns one of "RS" / "EMERALD" / "FRLG", or "UNKNOWN" if the code
    isn't a recognised Gen 3 variant or the buffer is too short.
    """
    if len(rom_bytes) < 0xB0:
        return "UNKNOWN"
    code = bytes(rom_bytes[0xAC:0xB0])
    return _GAME_CODE_TO_VARIANT.get(code, "UNKNOWN")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="Extract Pokemon front sprites from a user-supplied Gen 3 GBA ROM.",
    )
    parser.add_argument("--rom", required=True, help="path to Gen 3 .gba ROM")
    parser.add_argument("--out", required=True, help="output directory for PNGs")
    args = parser.parse_args(argv)
    try:
        n = extract_all(args.rom, args.out)
    except ValueError as exc:
        print(f"extract_sprites: {exc}", file=sys.stderr)
        return 2
    if n == 0:
        print(f"extract_sprites: {args.out} already populated — nothing to do")
    else:
        print(f"extract_sprites: wrote {n} sprites to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
