#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify cancelling Pokerus modal on Gen II Box 1 slot 0 leaves Pokerus NEVER."""

from gbc_harness import fail, load_lib, pk, run_gbc_lua

LUA = """
h.tap(h.KEY.DOWN,   30)
h.tap(h.KEY.DOWN,   60)
h.tap(h.KEY.DOWN,   90)
h.tap(h.KEY.A,      120)  -- Enter Boxes
h.tap(h.KEY.SELECT, 150)  -- Box 1 (index 1)
h.tap(h.KEY.A,      180)  -- Enter Pokemon Edit
h.tap(h.KEY.DOWN,   210)
h.tap(h.KEY.DOWN,   240)
h.tap(h.KEY.DOWN,   270)
h.tap(h.KEY.DOWN,   300)
h.tap(h.KEY.DOWN,   330)
h.tap(h.KEY.DOWN,   360)
h.tap(h.KEY.DOWN,   390)  -- ROW_POKERUS
h.tap(h.KEY.A,      420)  -- Open Pokerus modal
h.tap(h.KEY.RIGHT,  450)  -- NEVER -> ACTIVE
h.tap(h.KEY.B,      480)  -- Cancel modal
h.tap(h.KEY.B,      510)  -- Back to Boxes
h.tap(h.KEY.B,      540)  -- Back to Main menu
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    lib = load_lib()
    orig, dumped, res, _ = run_gbc_lua(LUA, frames=600, fixture_name="gs.sav")
    if dumped != orig:
        fail("gs.sav: cancelled Box Pokerus modal mutated SRAM", res)
    game = pk.detect(dumped, lib=lib)
    save = pk.open(dumped, game=game, lib=lib)
    try:
        pkrs = pk.get_box_pokerus(save, 1, 0)
    finally:
        pk.close(save)
    if pkrs.status != pk.PokerusStatus.NEVER:
        fail(f"expected NEVER, got {pkrs.status}", res)
    print("OK: Gen II Box Pokerus modal cancel passed")


if __name__ == "__main__":
    main()
