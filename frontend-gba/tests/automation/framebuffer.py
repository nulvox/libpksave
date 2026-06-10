# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Python helpers for the binary-PPM framebuffer dumps emitted by
`fb_dump_ppm` (defined in docker/test-gba/mgba-lua.c) and called from
Lua via `h.fb_dump(path)`.

Why PPM and not PNG: the harness runs on stdlib only — no Pillow on the
host, no zlib unpacking of IDAT chunks. Binary PPM (P6) is a 3-line ASCII
header followed by raw RGB bytes; the parser is ~15 lines.

Why bytes and not Pillow `Image` objects: the dirty-indicator assertion
is structural, not visual ("did the header strip change between snapshot
A and B?"). Region hashes + non-background-pixel counts are enough to
catch a glyph appearing or disappearing without dragging in a graphics
library.

GBA framebuffer is always 240×160. The PPM body is row-major RGB triples,
3 bytes per pixel, top-left origin (PPM convention matches mGBA's). The
header is `P6\\n<w> <h>\\n<maxval>\\n` followed by the binary body.
"""

import hashlib
import re
import struct
from dataclasses import dataclass
from typing import Iterable, Tuple

GBA_WIDTH = 240
GBA_HEIGHT = 160
BYTES_PER_PIXEL = 3
EXPECTED_BODY_LEN = GBA_WIDTH * GBA_HEIGHT * BYTES_PER_PIXEL  # 115_200

# PPM (P6) header: "P6\n240 160\n255\n" — strict parse.
_PPM_HEADER_RE = re.compile(rb"P6\n(\d+) (\d+)\n(\d+)\n")


@dataclass(frozen=True)
class Framebuffer:
    """A decoded PPM framebuffer: dimensions plus raw RGB body bytes."""

    width: int
    height: int
    pixels: bytes  # length == width * height * 3, row-major RGB

    @classmethod
    def load(cls, path: str) -> "Framebuffer":
        with open(path, "rb") as f:
            data = f.read()
        m = _PPM_HEADER_RE.match(data)
        if not m:
            raise ValueError(f"{path}: not a P6 PPM (header mismatch)")
        w, h, maxval = int(m.group(1)), int(m.group(2)), int(m.group(3))
        if maxval != 255:
            raise ValueError(f"{path}: expected 8-bit PPM (maxval=255), got {maxval}")
        body = data[m.end():]
        expected = w * h * 3
        if len(body) != expected:
            raise ValueError(
                f"{path}: PPM body length {len(body)} != expected "
                f"{expected} for {w}x{h}; truncated dump?"
            )
        return cls(width=w, height=h, pixels=body)

    def region(self, x: int, y: int, w: int, h: int) -> bytes:
        """Return the raw RGB bytes of a sub-region, row-major."""
        if x < 0 or y < 0 or x + w > self.width or y + h > self.height:
            raise ValueError(
                f"region ({x},{y},{w}x{h}) escapes framebuffer "
                f"{self.width}x{self.height}"
            )
        out = bytearray(w * h * 3)
        src_stride = self.width * 3
        dst_stride = w * 3
        for row in range(h):
            src_off = (y + row) * src_stride + x * 3
            dst_off = row * dst_stride
            out[dst_off:dst_off + dst_stride] = self.pixels[src_off:src_off + dst_stride]
        return bytes(out)

    def region_hash(self, x: int, y: int, w: int, h: int) -> str:
        """Stable short hash (first 16 hex chars of SHA-256) of a region."""
        return hashlib.sha256(self.region(x, y, w, h)).hexdigest()[:16]

    def count_non_black(self, x: int = 0, y: int = 0,
                        w: int | None = None, h: int | None = None) -> int:
        """Count pixels in the region that are not pure RGB (0,0,0).

        Black is butano's default backdrop. Non-black ≈ "something is
        drawn here" — text glyphs, cursor arrow, sprite. Lossy but
        cheap. For finer assertions, use `region_hash`.
        """
        if w is None:
            w = self.width - x
        if h is None:
            h = self.height - y
        blob = self.region(x, y, w, h)
        count = 0
        for i in range(0, len(blob), 3):
            if blob[i] or blob[i + 1] or blob[i + 2]:
                count += 1
        return count


# ── main_menu_scene header geometry ─────────────────────────────────────────
#
# main_menu_scene::redraw() in frontend-gba/src/scenes/main_menu_scene.cpp:58
# generates the header text centred at (x=0, y=-64). butano's bn::camera
# default origin is screen-centre, so y=-64 maps to display row 80-64 = 16
# (screen-centre y is 80 = 160/2). text_generator uses an 8px-tall glyph
# strip, so the rendered text occupies roughly rows 12 .. 20.
#
# We widen to a 0..32 strip to be insensitive to one-off glyph-rendering
# tweaks (sprite alignment, sub-pixel jitter). The strip still excludes
# the menu list below (which starts at y=-24 → row 56), so changes to
# the cursor row don't leak into the assertion.
HEADER_STRIP_Y0 = 0
HEADER_STRIP_Y1 = 32
HEADER_STRIP_HEIGHT = HEADER_STRIP_Y1 - HEADER_STRIP_Y0


def header_strip_hash(fb: Framebuffer) -> str:
    """SHA-256 of the top strip where main_menu renders its header text.

    Used by the dirty-indicator test: a clean save vs. dirty save differ
    only by a " *" suffix on the centered header string, which both
    adds non-black pixels AND shifts the existing glyphs leftward (centre
    alignment). Either change perturbs the hash, so equality means
    "header text is identical".
    """
    return fb.region_hash(0, HEADER_STRIP_Y0, fb.width, HEADER_STRIP_HEIGHT)


def header_strip_non_black(fb: Framebuffer) -> int:
    """Non-black pixel count in the header strip — an additive proxy.

    Adding " *" to "POKEMON RUBY" adds glyph ink (more non-black pixels)
    even after centre-realignment, because the asterisk is wider than
    the implicit trailing whitespace was. Use alongside header_strip_hash
    to give a directional assertion ("dirty has MORE ink than clean").
    """
    return fb.count_non_black(0, HEADER_STRIP_Y0, fb.width, HEADER_STRIP_HEIGHT)
