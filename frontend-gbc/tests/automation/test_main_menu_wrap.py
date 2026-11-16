#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify DOWNx5 on the 5-item main menu wraps back to Trainer (index 0)."""

from gbc_harness import fail, load_lib, pk, run_gbc_lua, validate_checksum

LUA = """
-- Wrap cursor around 5 entries: DOWN x 5 -> back on Trainer (row 0)
h.tap(h.KEY.DOWN, 30)
h.tap(h.KEY.DOWN, 60)
h.tap(h.KEY.DOWN, 90)
h.tap(h.KEY.DOWN, 120)
h.tap(h.KEY.DOWN, 150)

-- Enter Trainer, edit TID +10, return, Save & Exit
h.tap(h.KEY.A,     180)
h.tap(h.KEY.DOWN,  210)
h.tap(h.KEY.RIGHT, 240)
h.tap(h.KEY.B,     270)
h.tap(h.KEY.DOWN,  300)
h.tap(h.KEY.DOWN,  330)
h.tap(h.KEY.DOWN,  360)
h.tap(h.KEY.DOWN,  390)
h.tap(h.KEY.A,     420)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    lib = load_lib()
    _, dumped, res, _ = run_gbc_lua(LUA, frames=480, fixture_name="rb.sav")
    game = pk.detect(dumped, lib=lib)
    if game != pk.Game.RB:
        fail(f"expected RB, got {game}", res)
    save = pk.open(dumped, game=game, lib=lib)
    try:
        trainer = pk.get_trainer(save)
    finally:
        pk.close(save)
    if trainer.tid != 0x1234 + 10:
        fail(f"expected TID {0x1234 + 10}, got {trainer.tid}", res)
    if validate_checksum(lib, dumped, game) != 0:
        fail("checksum invalid after flush", res)
    print("OK: main_menu cursor wrapped after 5 DOWN taps")


if __name__ == "__main__":
    main()
