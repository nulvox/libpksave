#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify Party scene navigation produces zero SRAM writes."""

from gbc_harness import fail, run_gbc_lua

LUA = """
h.tap(h.KEY.DOWN, 30)   -- Main menu -> Party
h.tap(h.KEY.A,    60)   -- Enter Party
h.tap(h.KEY.DOWN, 90)   -- Slot 0 -> Slot 1
h.tap(h.KEY.DOWN, 120)  -- Slot 1 -> Slot 2
h.tap(h.KEY.UP,   150)  -- Slot 2 -> Slot 1
h.tap(h.KEY.B,    180)  -- Back to Main menu
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    orig, dumped, res, _ = run_gbc_lua(LUA, frames=240, fixture_name="rb.sav")
    if dumped != orig:
        fail("party_scene navigation mutated SRAM", res)
    print("OK: party_scene navigation left SRAM untouched")


if __name__ == "__main__":
    main()
