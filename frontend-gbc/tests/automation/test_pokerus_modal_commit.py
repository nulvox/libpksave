#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify committing Pokerus modal on Gen II party slot 0 sets ACTIVE(1, 1)."""

from gbc_harness import GEN2_CASES, fail, load_lib, pk, run_gbc_lua, validate_checksum

LUA = """
h.tap(h.KEY.DOWN,  30)   -- Main menu -> Party
h.tap(h.KEY.A,     60)   -- Enter Party
h.tap(h.KEY.A,     90)   -- Enter Pokemon Edit
h.tap(h.KEY.DOWN,  120)
h.tap(h.KEY.DOWN,  150)
h.tap(h.KEY.DOWN,  180)
h.tap(h.KEY.DOWN,  210)
h.tap(h.KEY.DOWN,  240)
h.tap(h.KEY.DOWN,  270)
h.tap(h.KEY.DOWN,  300)  -- ROW_POKERUS
h.tap(h.KEY.A,     330)  -- Open Pokerus modal
h.tap(h.KEY.RIGHT, 360)  -- Cycle NEVER -> ACTIVE (strain=1, days=1)
h.tap(h.KEY.A,     390)  -- Commit modal
h.tap(h.KEY.B,     420)  -- Back to Party
h.tap(h.KEY.B,     450)  -- Back to Main menu
h.tap(h.KEY.DOWN,  480)
h.tap(h.KEY.DOWN,  510)
h.tap(h.KEY.DOWN,  540)
h.tap(h.KEY.DOWN,  570)  -- Save & Exit
h.tap(h.KEY.A,     600)  -- Save
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    lib = load_lib()
    for fixture_name, expected_game in GEN2_CASES:
        _, dumped, res, _ = run_gbc_lua(LUA, frames=660, fixture_name=fixture_name)
        game = pk.detect(dumped, lib=lib)
        save = pk.open(dumped, game=game, lib=lib)
        try:
            pkrs = pk.get_party_pokerus(save, 0)
        finally:
            pk.close(save)
        if pkrs.status != pk.PokerusStatus.ACTIVE or pkrs.strain != 1 or pkrs.days != 1:
            fail(
                f"{fixture_name}: expected ACTIVE(1,1), "
                f"got status={pkrs.status} strain={pkrs.strain} days={pkrs.days}",
                res,
            )
        if validate_checksum(lib, dumped, game) != 0:
            fail(f"{fixture_name}: checksum invalid after flush", res)
        print(f"  {fixture_name}: pokerus ACTIVE(1,1) OK")
    print("OK: Gen II party Pokerus modal commit passed")


if __name__ == "__main__":
    main()
