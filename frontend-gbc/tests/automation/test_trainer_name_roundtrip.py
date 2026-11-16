#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify trainer name edit ('RED' -> 'REDAAA') across all 4 Gen I/II variants."""

from gbc_harness import ALL_CASES, fail, load_lib, pk, run_gbc_lua, validate_checksum

LUA = """
h.tap(h.KEY.A,     30)   -- Main menu -> Trainer
h.tap(h.KEY.A,     60)   -- Trainer -> Name text_input
h.tap(h.KEY.A,     90)   -- 'A'
h.tap(h.KEY.A,     120)  -- 'A'
h.tap(h.KEY.A,     150)  -- 'A'
h.tap(h.KEY.START, 180)  -- Commit
h.tap(h.KEY.B,     210)  -- Back to Main menu
h.tap(h.KEY.DOWN,  240)
h.tap(h.KEY.DOWN,  270)
h.tap(h.KEY.DOWN,  300)
h.tap(h.KEY.DOWN,  330)  -- Save & Exit
h.tap(h.KEY.A,     360)  -- Save
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    lib = load_lib()
    for fixture_name, expected_game in ALL_CASES:
        _, dumped, res, _ = run_gbc_lua(LUA, frames=420, fixture_name=fixture_name)
        game = pk.detect(dumped, lib=lib)
        if game != expected_game:
            fail(f"{fixture_name}: expected {expected_game.name}, got {game.name}", res)
        save = pk.open(dumped, game=game, lib=lib)
        try:
            name = pk.get_trainer_name(save)
        finally:
            pk.close(save)
        if name != "REDAAA":
            fail(f"{fixture_name}: expected 'REDAAA', got {name!r}", res)
        if validate_checksum(lib, dumped, game) != 0:
            fail(f"{fixture_name}: checksum invalid after flush", res)
        print(f"  {fixture_name}: trainer_name={name!r} OK")
    print("OK: 4-variant trainer name round-trip passed")


if __name__ == "__main__":
    main()
