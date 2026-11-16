#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify party slot 0 STATS EV HP (stat exp) edit (0 -> 40) across 4 variants."""

from gbc_harness import ALL_CASES, fail, load_lib, pk, run_gbc_lua, validate_checksum

LUA = """
h.tap(h.KEY.DOWN,  30)   -- Main menu -> Party
h.tap(h.KEY.A,     60)   -- Enter Party
h.tap(h.KEY.A,     90)   -- Enter Pokemon Edit
h.tap(h.KEY.RIGHT, 120)  -- Switch to STATS tab
h.tap(h.KEY.DOWN,  150)  -- ROW_LEVEL -> ROW_EV_HP
h.tap(h.KEY.A,     180)  -- Enter MODE_EDIT on EV HP
h.tap(h.KEY.RIGHT, 210)  -- +10 -> 10
h.tap(h.KEY.RIGHT, 240)  -- +10 -> 20
h.tap(h.KEY.RIGHT, 270)  -- +10 -> 30
h.tap(h.KEY.RIGHT, 300)  -- +10 -> 40
h.tap(h.KEY.A,     330)  -- Exit MODE_EDIT
h.tap(h.KEY.B,     360)  -- Back to Party
h.tap(h.KEY.B,     390)  -- Back to Main menu
h.tap(h.KEY.DOWN,  420)
h.tap(h.KEY.DOWN,  450)
h.tap(h.KEY.DOWN,  480)
h.tap(h.KEY.DOWN,  510)  -- Save & Exit
h.tap(h.KEY.A,     540)  -- Save
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    lib = load_lib()
    for fixture_name, expected_game in ALL_CASES:
        _, dumped, res, _ = run_gbc_lua(LUA, frames=600, fixture_name=fixture_name)
        game = pk.detect(dumped, lib=lib)
        save = pk.open(dumped, game=game, lib=lib)
        try:
            party, _ = pk.get_party(save)
        finally:
            pk.close(save)
        if party[0].evs[0] != 40:
            fail(f"{fixture_name}: expected evs[0] == 40, got {party[0].evs[0]}", res)
        if validate_checksum(lib, dumped, game) != 0:
            fail(f"{fixture_name}: checksum invalid after flush", res)
        print(f"  {fixture_name}: evs[0]={party[0].evs[0]} OK")
    print("OK: 4-variant stats EV round-trip passed")


if __name__ == "__main__":
    main()
