#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify party slot 0 nickname edit ('BULBASAUR' -> 'BULBASAURA') across 4 variants."""

from gbc_harness import fail, load_lib, pk, run_gbc_lua, validate_checksum

LUA = """
h.tap(h.KEY.DOWN,  30)   -- Main menu -> Party
h.tap(h.KEY.A,     60)   -- Enter Party
h.tap(h.KEY.A,     90)   -- Enter Pokemon Edit (slot 0)
h.tap(h.KEY.DOWN,  120)  -- ROW_SPECIES -> ROW_NICKNAME
h.tap(h.KEY.A,     150)  -- Open text_input
h.tap(h.KEY.A,     180)  -- Append 'A' (9 -> 10 chars)
h.tap(h.KEY.START, 210)  -- Commit nickname
h.tap(h.KEY.B,     240)  -- Back to Party
h.tap(h.KEY.B,     270)  -- Back to Main menu
h.tap(h.KEY.DOWN,  300)
h.tap(h.KEY.DOWN,  330)
h.tap(h.KEY.DOWN,  360)
h.tap(h.KEY.DOWN,  390)  -- Save & Exit
h.tap(h.KEY.A,     420)  -- Save
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


CASES = [
    ("rb.sav",      pk.Game.RB,      "BULBASAURA"),
    ("yellow.sav",  pk.Game.YELLOW,  "BULBASAURA"),
    ("gs.sav",      pk.Game.GS,      "A"),
    ("crystal.sav", pk.Game.CRYSTAL, "A"),
]


def main():
    lib = load_lib()
    for fixture_name, expected_game, expected_nick in CASES:
        _, dumped, res, _ = run_gbc_lua(LUA, frames=480, fixture_name=fixture_name)
        game = pk.detect(dumped, lib=lib)
        save = pk.open(dumped, game=game, lib=lib)
        try:
            party, _ = pk.get_party(save)
        finally:
            pk.close(save)
        nick = party[0].nickname.decode("ascii", errors="replace")
        if nick != expected_nick:
            fail(f"{fixture_name}: expected {expected_nick!r}, got {nick!r}", res)
        if validate_checksum(lib, dumped, game) != 0:
            fail(f"{fixture_name}: checksum invalid after flush", res)
        print(f"  {fixture_name}: party[0].nickname={nick!r} OK")
    print("OK: 4-variant party nickname round-trip passed")


if __name__ == "__main__":
    main()
