#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify save_scene renders 'SAVING...' splash and ignores D-pad input during save."""

import framebuffer as fbm
from gbc_harness import fail, run_gbc_lua

LUA = """
h.at(25, function() h.fb_dump("/dump/fb_pre.ppm") end)

-- Move to Save & Exit (DOWN x 4) and press A at frame 150
h.tap(h.KEY.DOWN, 30)
h.tap(h.KEY.DOWN, 60)
h.tap(h.KEY.DOWN, 90)
h.tap(h.KEY.DOWN, 120)
h.tap(h.KEY.A,    150)

-- Snapshot during the 20-frame save splash and inject D-pad DOWN taps
h.at(160, function() h.fb_dump("/dump/fb_saving.ppm") end)
h.tap(h.KEY.DOWN, 155)
h.tap(h.KEY.DOWN, 162)

-- After save_scene returns to main_menu (cursor reset to row 0)
h.at(225, function() h.fb_dump("/dump/fb_post.ppm") end)
"""


def main():
    _, _, res, extras = run_gbc_lua(
        LUA,
        frames=240,
        fixture_name="rb.sav",
        extra_dumps=["fb_pre.ppm", "fb_saving.ppm", "fb_post.ppm"],
    )
    for name in ("fb_pre.ppm", "fb_saving.ppm", "fb_post.ppm"):
        if name not in extras:
            fail(f"missing framebuffer dump {name}", res)

    fb_pre = fbm.Framebuffer.from_bytes(extras["fb_pre.ppm"], "fb_pre.ppm")
    fb_saving = fbm.Framebuffer.from_bytes(extras["fb_saving.ppm"], "fb_saving.ppm")
    fb_post = fbm.Framebuffer.from_bytes(extras["fb_post.ppm"], "fb_post.ppm")

    h_pre = fb_pre.region_hash(0, 0, fb_pre.width, fb_pre.height)
    h_saving = fb_saving.region_hash(0, 0, fb_saving.width, fb_saving.height)
    h_post = fb_post.region_hash(0, 0, fb_post.width, fb_post.height)

    if h_saving == h_pre:
        fail("save_scene splash did not render (saving hash == pre hash)", res)
    if h_post != h_pre:
        fail(
            f"post-save main_menu hash ({h_post}) != pre-save ({h_pre}); "
            "injected D-pad input leaked through save_scene",
            res,
        )
    print(f"OK: save_scene blocked input (PRE={h_pre} SAVING={h_saving} POST={h_post})")


if __name__ == "__main__":
    main()
