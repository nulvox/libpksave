#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify TID numeric_field +30 round-trip across all 4 Gen I/II variants."""

from gbc_harness import ALL_CASES, fail, load_lib, pk, run_gbc_lua, validate_checksum

LUA = """
h.tap(h.KEY.A,     30)   -- Main menu -> Trainer
h.tap(h.KEY.DOWN,  60)   -- Name -> TID
h.tap(h.KEY.RIGHT, 90)   -- +10
h.tap(h.KEY.RIGHT, 120)  -- +10
h.tap(h.KEY.RIGHT, 150)  -- +10
h.tap(h.KEY.B,     180)  -- Back to Main menu
h.tap(h.KEY.DOWN,  210)
h.tap(h.KEY.DOWN,  240)
h.tap(h.KEY.DOWN,  270)
h.tap(h.KEY.DOWN,  300)  -- Save & Exit
h.tap(h.KEY.A,     330)  -- Save
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    lib = load_lib()
    for fixture_name, expected_game in ALL_CASES:
        _, dumped, res, _ = run_gbc_lua(LUA, frames=400, fixture_name=fixture_name)
        game = pk.detect(dumped, lib=lib)
        if game != expected_game:
            fail(f"{fixture_name}: expected {expected_game.name}, got {game.name}", res)
        save = pk.open(dumped, game=game, lib=lib)
        try:
            tr = pk.get_trainer(save)
            name = pk.get_trainer_name(save)
        finally:
            pk.close(save)
        expected_tid = 0x1234 + 30
        if tr.tid != expected_tid:
            fail(f"{fixture_name}: expected TID {expected_tid}, got {tr.tid}", res)
        if name != "RED":
            fail(f"{fixture_name}: expected trainer name 'RED' preserved, got {name!r}", res)
        if validate_checksum(lib, dumped, game) != 0:
            fail(f"{fixture_name}: checksum invalid after flush", res)
        print(f"  {fixture_name}: tid={tr.tid:#06x} name={name!r} OK")
    print("OK: 4-variant TID round-trip passed")


if __name__ == "__main__":
    main()
