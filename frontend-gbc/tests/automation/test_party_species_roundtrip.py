#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify party slot 0 species edit (0x99 -> 0x9A) across all 4 Gen I/II variants."""

from gbc_harness import ALL_CASES, fail, load_lib, pk, run_gbc_lua, validate_checksum

LUA = """
h.tap(h.KEY.DOWN,  30)   -- Main menu -> Party
h.tap(h.KEY.A,     60)   -- Enter Party
h.tap(h.KEY.A,     90)   -- Enter Pokemon Edit (slot 0, ROW_SPECIES)
h.tap(h.KEY.A,     120)  -- Enter MODE_EDIT on Species
h.tap(h.KEY.RIGHT, 150)  -- Step species +1 (0x99 -> 0x9A)
h.tap(h.KEY.A,     180)  -- Exit MODE_EDIT
h.tap(h.KEY.B,     210)  -- Back to Party
h.tap(h.KEY.B,     240)  -- Back to Main menu
h.tap(h.KEY.DOWN,  270)
h.tap(h.KEY.DOWN,  300)
h.tap(h.KEY.DOWN,  330)
h.tap(h.KEY.DOWN,  360)  -- Save & Exit
h.tap(h.KEY.A,     390)  -- Save
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    lib = load_lib()
    for fixture_name, expected_game in ALL_CASES:
        _, dumped, res, _ = run_gbc_lua(LUA, frames=460, fixture_name=fixture_name)
        game = pk.detect(dumped, lib=lib)
        if game != expected_game:
            fail(f"{fixture_name}: expected {expected_game.name}, got {game.name}", res)
        save = pk.open(dumped, game=game, lib=lib)
        try:
            party, count = pk.get_party(save)
        finally:
            pk.close(save)
        if count < 1 or party[0].species != 0x9A:
            fail(
                f"{fixture_name}: expected party[0].species == 0x9A (154), "
                f"got {party[0].species:#04x}",
                res,
            )
        if validate_checksum(lib, dumped, game) != 0:
            fail(f"{fixture_name}: checksum invalid after flush", res)
        print(f"  {fixture_name}: party[0].species=0x{party[0].species:02X} OK")
    print("OK: 4-variant party species round-trip passed")


if __name__ == "__main__":
    main()
