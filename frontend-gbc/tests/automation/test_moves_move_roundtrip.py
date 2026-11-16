#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify party slot 0 Move 0 edit (33 Tackle -> 34 Body Slam, PP -> 15) across 4 variants."""

from gbc_harness import ALL_CASES, fail, load_lib, pk, run_gbc_lua, validate_checksum

LUA = """
h.tap(h.KEY.DOWN,  30)   -- Main menu -> Party
h.tap(h.KEY.A,     60)   -- Enter Party
h.tap(h.KEY.A,     90)   -- Enter Pokemon Edit
h.tap(h.KEY.RIGHT, 120)  -- STATS
h.tap(h.KEY.RIGHT, 150)  -- MOVES (ROW_MOVE_0 = 0)
h.tap(h.KEY.A,     180)  -- Enter MODE_EDIT on Move 0
h.tap(h.KEY.RIGHT, 210)  -- Step move 33 -> 34 (Body Slam, base PP 15)
h.tap(h.KEY.A,     240)  -- Exit MODE_EDIT
h.tap(h.KEY.B,     270)  -- Back to Party
h.tap(h.KEY.B,     300)  -- Back to Main menu
h.tap(h.KEY.DOWN,  330)
h.tap(h.KEY.DOWN,  360)
h.tap(h.KEY.DOWN,  390)
h.tap(h.KEY.DOWN,  420)  -- Save & Exit
h.tap(h.KEY.A,     450)  -- Save
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    lib = load_lib()
    for fixture_name, expected_game in ALL_CASES:
        _, dumped, res, _ = run_gbc_lua(LUA, frames=510, fixture_name=fixture_name)
        game = pk.detect(dumped, lib=lib)
        save = pk.open(dumped, game=game, lib=lib)
        try:
            party, _ = pk.get_party(save)
        finally:
            pk.close(save)
        if party[0].move_ids[0] != 34:
            fail(f"{fixture_name}: expected move_ids[0] == 34, got {party[0].move_ids[0]}", res)
        if party[0].move_pp[0] != 15:
            fail(f"{fixture_name}: expected move_pp[0] == 15, got {party[0].move_pp[0]}", res)
        if validate_checksum(lib, dumped, game) != 0:
            fail(f"{fixture_name}: checksum invalid after flush", res)
        print(
            f"  {fixture_name}: move_ids[0]={party[0].move_ids[0]} "
            f"move_pp[0]={party[0].move_pp[0]} OK"
        )
    print("OK: 4-variant moves move round-trip passed")


if __name__ == "__main__":
    main()
