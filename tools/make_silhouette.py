#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Generate the generic 64x64 silhouette BMP used as the placeholder when no
extracted sprite is available for a species. Original art, MIT-licensed —
the repository ships this silhouette so `make gba` (no args) produces a
working ROM with no Game Freak assets.

Format is indexed 4bpp BMP (16-color palette, multiple-of-8 dims) because
butano 18.5's grit only consumes BMP, not PNG.

Usage:
    python3 tools/make_silhouette.py [output_path]

Default output: frontend-gba/graphics/silhouette.bmp
"""

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import extract_sprites as ex  # noqa: E402 — reuse the indexed-BMP writer

# Palette: index 0 is the GBA transparency convention (magenta = #FF00FF).
# Index 1 is the silhouette fill. Remaining entries are a grayscale ramp so
# butano sees a well-formed 16-entry palette; they go unused at render time.
_PALETTE = [(255, 0, 255), (32, 32, 32)] + \
           [(c, c, c) for c in (16, 48, 64, 80, 96, 112, 128, 144,
                                 160, 176, 192, 208, 224, 240)]


def _filled_ellipse(pixels, cx, cy, rx, ry, color):
    for y in range(64):
        dy = (y - cy) / ry
        if dy * dy >= 1:
            continue
        for x in range(64):
            dx = (x - cx) / rx
            if dx * dx + dy * dy <= 1:
                pixels[y * 64 + x] = color


def _build_silhouette() -> bytes:
    pixels = bytearray(64 * 64)  # all 0 (transparent)
    # Body — squat oval centered low
    _filled_ellipse(pixels, cx=32, cy=42, rx=20, ry=13, color=1)
    # Head — circle on top
    _filled_ellipse(pixels, cx=32, cy=24, rx=13, ry=13, color=1)
    # Ears — small bumps on top of the head
    _filled_ellipse(pixels, cx=22, cy=13, rx=4, ry=6, color=1)
    _filled_ellipse(pixels, cx=42, cy=13, rx=4, ry=6, color=1)
    # Feet — bumps at the base
    _filled_ellipse(pixels, cx=23, cy=54, rx=5, ry=4, color=1)
    _filled_ellipse(pixels, cx=41, cy=54, rx=5, ry=4, color=1)
    return bytes(pixels)


def main(argv=None) -> int:
    argv = sys.argv[1:] if argv is None else argv
    if argv:
        out_path = argv[0]
    else:
        repo_root = os.path.abspath(os.path.join(_HERE, ".."))
        out_path = os.path.join(repo_root, "frontend-gba", "graphics", "silhouette.bmp")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    pixels = _build_silhouette()
    ex.write_indexed_bmp_4bpp(out_path, 64, 64, pixels, _PALETTE)
    print(f"silhouette: wrote {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
