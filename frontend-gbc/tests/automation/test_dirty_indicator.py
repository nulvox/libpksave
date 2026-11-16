#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify GBC header strip CLEAN -> DIRTY -> AFTER_SAVE hash cycle via fb_dump_ppm."""

import framebuffer as fbm
from gbc_harness import fail, run_gbc_lua

LUA = """
h.at(25, function() h.fb_dump("/dump/fb_clean.ppm") end)

-- Enter Trainer, edit TID +10, return to Main menu
h.tap(h.KEY.A,     30)
h.tap(h.KEY.DOWN,  60)
h.tap(h.KEY.RIGHT, 90)
h.tap(h.KEY.B,     120)

h.at(145, function() h.fb_dump("/dump/fb_dirty.ppm") end)

-- Navigate to Save & Exit (DOWN x 4) and press A
h.tap(h.KEY.DOWN, 160)
h.tap(h.KEY.DOWN, 190)
h.tap(h.KEY.DOWN, 220)
h.tap(h.KEY.DOWN, 250)
h.tap(h.KEY.A,    280)

h.at(360, function() h.fb_dump("/dump/fb_after_save.ppm") end)
"""


def main():
    _, _, res, extras = run_gbc_lua(
        LUA,
        frames=380,
        fixture_name="rb.sav",
        extra_dumps=["fb_clean.ppm", "fb_dirty.ppm", "fb_after_save.ppm"],
    )
    for name in ("fb_clean.ppm", "fb_dirty.ppm", "fb_after_save.ppm"):
        if name not in extras:
            fail(f"missing framebuffer dump {name}", res)

    fb_clean = fbm.Framebuffer.from_bytes(extras["fb_clean.ppm"], "fb_clean.ppm")
    fb_dirty = fbm.Framebuffer.from_bytes(extras["fb_dirty.ppm"], "fb_dirty.ppm")
    fb_after = fbm.Framebuffer.from_bytes(
        extras["fb_after_save.ppm"], "fb_after_save.ppm"
    )

    h_clean = fbm.header_strip_hash(fb_clean)
    h_dirty = fbm.header_strip_hash(fb_dirty)
    h_after = fbm.header_strip_hash(fb_after)

    if h_clean == h_dirty:
        fail(f"expected DIRTY header hash != CLEAN ({h_clean})", res)
    if h_after != h_clean:
        fail(
            f"expected AFTER_SAVE header hash ({h_after}) == CLEAN ({h_clean})",
            res,
        )
    print(f"OK: dirty indicator cycle CLEAN={h_clean} DIRTY={h_dirty} AFTER={h_after}")


if __name__ == "__main__":
    main()
