#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify party slot 0 Move 0 PP edit (35 -> 34) across 4 variants."""

from gbc_harness import ALL_CASES, fail, load_lib, pk, run_gbc_lua, validate_checksum

LUA = """
h.tap(h.KEY.DOWN,  30)   -- Main menu -> Party
h.tap(h.KEY.A,     60)   -- Enter Party
h.tap(h.KEY.A,     90)   -- Enter Pokemon Edit
h.tap(h.KEY.RIGHT, 120)  -- STATS
h.tap(h.KEY.RIGHT, 150)  -- MOVES
h.tap(h.KEY.DOWN,  180)  -- ROW_MOVE_0 -> ROW_PP_0
h.tap(h.KEY.A,     210)  -- Enter MODE_EDIT on PP 0
h.tap(h.KEY.DOWN,  240)  -- -1 PP (35 -> 34)
h.tap(h.KEY.A,     270)  -- Exit MODE_EDIT
h.tap(h.KEY.B,     300)  -- Back to Party
h.tap(h.KEY.B,     330)  -- Back to Main menu
h.tap(h.KEY.DOWN,  360)
h.tap(h.KEY.DOWN,  390)
h.tap(h.KEY.DOWN,  420)
h.tap(h.KEY.DOWN,  450)  -- Save & Exit
h.tap(h.KEY.A,     480)  -- Save
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    lib = load_lib()
    for fixture_name, expected_game in ALL_CASES:
        _, dumped, res, _ = run_gbc_lua(LUA, frames=540, fixture_name=fixture_name)
        game = pk.detect(dumped, lib=lib)
        save = pk.open(dumped, game=game, lib=lib)
        try:
            party, _ = pk.get_party(save)
        finally:
            pk.close(save)
        if party[0].move_pp[0] != 34:
            fail(f"{fixture_name}: expected move_pp[0] == 34, got {party[0].move_pp[0]}", res)
        if validate_checksum(lib, dumped, game) != 0:
            fail(f"{fixture_name}: checksum invalid after flush", res)
        print(f"  {fixture_name}: move_pp[0]={party[0].move_pp[0]} OK")
    print("OK: 4-variant moves PP round-trip passed")


if __name__ == "__main__":
    main()
