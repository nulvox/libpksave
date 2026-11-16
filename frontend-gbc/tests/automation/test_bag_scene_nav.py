#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify Bag pocket cycling and slot navigation produce zero SRAM writes."""

from gbc_harness import fail, run_gbc_lua

LUA = """
h.tap(h.KEY.DOWN,  30)   -- Main menu -> Party
h.tap(h.KEY.DOWN,  60)   -- Party -> Bag
h.tap(h.KEY.A,     90)   -- Enter Bag
h.tap(h.KEY.RIGHT, 120)  -- ITEMS -> BALLS
h.tap(h.KEY.RIGHT, 150)  -- BALLS -> KEY
h.tap(h.KEY.RIGHT, 180)  -- KEY -> TM/HM
h.tap(h.KEY.LEFT,  210)  -- TM/HM -> KEY
h.tap(h.KEY.DOWN,  240)  -- Slot 0 -> Slot 1
h.tap(h.KEY.B,     270)  -- Back to Main menu
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    orig, dumped, res, _ = run_gbc_lua(LUA, frames=320, fixture_name="gs.sav")
    if dumped != orig:
        fail("bag_scene navigation mutated SRAM", res)
    print("OK: bag_scene navigation left SRAM untouched")


if __name__ == "__main__":
    main()
