# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""
PPM (P6) framebuffer helpers for the 160x144 GBC display.
"""

import hashlib
import re
from dataclasses import dataclass

GBC_WIDTH = 160
GBC_HEIGHT = 144

_PPM_HEADER_RE = re.compile(rb"P6\n(\d+) (\d+)\n(\d+)\n")


@dataclass(frozen=True)
class Framebuffer:
    width: int
    height: int
    pixels: bytes

    @classmethod
    def from_bytes(cls, data: bytes, label: str = "<bytes>") -> "Framebuffer":
        m = _PPM_HEADER_RE.match(data)
        if not m:
            raise ValueError(f"{label}: not a P6 PPM")
        w, h, maxval = int(m.group(1)), int(m.group(2)), int(m.group(3))
        if maxval != 255:
            raise ValueError(f"{label}: expected maxval=255, got {maxval}")
        body = data[m.end():]
        expected = w * h * 3
        if len(body) != expected:
            raise ValueError(f"{label}: body length {len(body)} != {expected}")
        return cls(width=w, height=h, pixels=body)

    @classmethod
    def load(cls, path: str) -> "Framebuffer":
        with open(path, "rb") as f:
            data = f.read()
        return cls.from_bytes(data, path)

    def region(self, x: int, y: int, w: int, h: int) -> bytes:
        if x < 0 or y < 0 or x + w > self.width or y + h > self.height:
            raise ValueError(
                f"region ({x},{y},{w}x{h}) out of {self.width}x{self.height}"
            )
        out = bytearray(w * h * 3)
        src_stride = self.width * 3
        dst_stride = w * 3
        for row in range(h):
            src_off = (y + row) * src_stride + x * 3
            dst_off = row * dst_stride
            out[dst_off:dst_off + dst_stride] = (
                self.pixels[src_off:src_off + dst_stride]
            )
        return bytes(out)

    def region_hash(self, x: int, y: int, w: int, h: int) -> str:
        return hashlib.sha256(self.region(x, y, w, h)).hexdigest()[:16]


def header_strip_hash(fb: Framebuffer) -> str:
    """Hash the top 8-pixel header row (tile row 0) of the 160x144 GBC screen."""
    return fb.region_hash(0, 0, fb.width, 8)
