#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify cancelling Pokerus modal on Gen II party slot 0 leaves Pokerus NEVER."""

from gbc_harness import GEN2_CASES, fail, load_lib, pk, run_gbc_lua

LUA = """
h.tap(h.KEY.DOWN,  30)   -- Main menu -> Party
h.tap(h.KEY.A,     60)   -- Enter Party
h.tap(h.KEY.A,     90)   -- Enter Pokemon Edit
h.tap(h.KEY.DOWN,  120)  -- 1
h.tap(h.KEY.DOWN,  150)  -- 2
h.tap(h.KEY.DOWN,  180)  -- 3
h.tap(h.KEY.DOWN,  210)  -- 4
h.tap(h.KEY.DOWN,  240)  -- 5
h.tap(h.KEY.DOWN,  270)  -- 6
h.tap(h.KEY.DOWN,  300)  -- 7 (ROW_POKERUS)
h.tap(h.KEY.A,     330)  -- Open Pokerus modal
h.tap(h.KEY.RIGHT, 360)  -- Cycle NEVER -> ACTIVE
h.tap(h.KEY.B,     390)  -- Cancel modal
h.tap(h.KEY.B,     420)  -- Back to Party
h.tap(h.KEY.B,     450)  -- Back to Main menu
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    lib = load_lib()
    for fixture_name, expected_game in GEN2_CASES:
        orig, dumped, res, _ = run_gbc_lua(LUA, frames=500, fixture_name=fixture_name)
        if dumped != orig:
            fail(f"{fixture_name}: cancelled Pokerus modal mutated SRAM", res)
        game = pk.detect(dumped, lib=lib)
        save = pk.open(dumped, game=game, lib=lib)
        try:
            pkrs = pk.get_party_pokerus(save, 0)
        finally:
            pk.close(save)
        if pkrs.status != pk.PokerusStatus.NEVER:
            fail(f"{fixture_name}: expected NEVER, got {pkrs.status}", res)
        print(f"  {fixture_name}: cancel preserved NEVER OK")
    print("OK: Gen II party Pokerus modal cancel passed")


if __name__ == "__main__":
    main()
