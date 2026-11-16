#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify Box cycling (SELECTx3) and slot navigation produce zero SRAM writes."""

from gbc_harness import fail, run_gbc_lua

LUA = """
h.tap(h.KEY.DOWN,   30)   -- Main menu -> Party
h.tap(h.KEY.DOWN,   60)   -- Party -> Bag
h.tap(h.KEY.DOWN,   90)   -- Bag -> Boxes
h.tap(h.KEY.A,      120)  -- Enter Boxes
h.tap(h.KEY.SELECT, 150)  -- Box 1 -> Box 2
h.tap(h.KEY.SELECT, 180)  -- Box 2 -> Box 3
h.tap(h.KEY.SELECT, 210)  -- Box 3 -> Box 4
h.tap(h.KEY.DOWN,   240)  -- Slot 0 -> Slot 1
h.tap(h.KEY.B,      270)  -- Back to Main menu
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    orig, dumped, res, _ = run_gbc_lua(LUA, frames=320, fixture_name="rb.sav")
    if dumped != orig:
        fail("box_scene navigation mutated SRAM", res)
    print("OK: box_scene navigation left SRAM untouched")


if __name__ == "__main__":
    main()
