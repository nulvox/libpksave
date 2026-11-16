#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify cycling Pokemon Edit tabs (IDENTITY -> STATS -> MOVES) leaves SRAM untouched."""

from gbc_harness import fail, run_gbc_lua

LUA = """
h.tap(h.KEY.DOWN,  30)   -- Main menu -> Party
h.tap(h.KEY.A,     60)   -- Enter Party
h.tap(h.KEY.A,     90)   -- Enter Pokemon Edit (IDENTITY)
h.tap(h.KEY.RIGHT, 120)  -- STATS
h.tap(h.KEY.RIGHT, 150)  -- MOVES
h.tap(h.KEY.LEFT,  180)  -- STATS
h.tap(h.KEY.LEFT,  210)  -- IDENTITY
h.tap(h.KEY.B,     240)  -- Back to Party
h.tap(h.KEY.B,     270)  -- Back to Main menu
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    orig, dumped, res, _ = run_gbc_lua(LUA, frames=320, fixture_name="rb.sav")
    if dumped != orig:
        fail("pokemon_edit tab navigation mutated SRAM", res)
    print("OK: pokemon_edit tab navigation left SRAM untouched")


if __name__ == "__main__":
    main()
