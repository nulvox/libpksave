#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify all 4 Gen I/II fixtures load into SRAM and decode after boot."""

from gbc_harness import ALL_CASES, fail, load_lib, pk, run_gbc_lua, validate_checksum

LUA = """
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    lib = load_lib()
    for fixture_name, expected_game in ALL_CASES:
        _, dumped, res, _ = run_gbc_lua(LUA, frames=60, fixture_name=fixture_name)
        game = pk.detect(dumped, lib=lib)
        if game != expected_game:
            fail(f"{fixture_name}: expected {expected_game.name}, got {game.name}", res)
        save = pk.open(dumped, game=game, lib=lib)
        try:
            name = pk.get_trainer_name(save)
        finally:
            pk.close(save)
        if name != "RED":
            fail(f"{fixture_name}: expected trainer name 'RED', got {name!r}", res)
        if validate_checksum(lib, dumped, game) != 0:
            fail(f"{fixture_name}: checksum invalid after boot", res)
        print(f"  {fixture_name}: game={game.name} trainer={name!r} OK")
    print("OK: 4-variant fixture boot round-trip passed")


if __name__ == "__main__":
    main()
