#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify committing Pokerus modal on Gen II Box 1 slot 0 sets ACTIVE(1, 1)."""

from gbc_harness import fail, load_lib, pk, run_gbc_lua, validate_checksum

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
h.tap(h.KEY.RIGHT,  450)  -- NEVER -> ACTIVE(1,1)
h.tap(h.KEY.A,      480)  -- Commit modal
h.tap(h.KEY.B,      510)  -- Back to Boxes
h.tap(h.KEY.B,      540)  -- Back to Main menu (cursor reset to 0)
h.tap(h.KEY.DOWN,   570)
h.tap(h.KEY.DOWN,   600)
h.tap(h.KEY.DOWN,   630)
h.tap(h.KEY.DOWN,   660)  -- Save & Exit
h.tap(h.KEY.A,      690)  -- Save
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    lib = load_lib()
    _, dumped, res, _ = run_gbc_lua(LUA, frames=750, fixture_name="gs.sav")
    game = pk.detect(dumped, lib=lib)
    save = pk.open(dumped, game=game, lib=lib)
    try:
        pkrs = pk.get_box_pokerus(save, 1, 0)
    finally:
        pk.close(save)
    if pkrs.status != pk.PokerusStatus.ACTIVE or pkrs.strain != 1 or pkrs.days != 1:
        fail(
            f"expected ACTIVE(1,1), got status={pkrs.status} "
            f"strain={pkrs.strain} days={pkrs.days}",
            res,
        )
    if validate_checksum(lib, dumped, game) != 0:
        fail("checksum invalid after flush", res)
    print("OK: Gen II Box Pokerus modal commit passed")


if __name__ == "__main__":
    main()
