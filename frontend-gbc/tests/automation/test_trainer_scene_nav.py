#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify navigating Trainer scene without editing produces zero SRAM writes."""

from gbc_harness import fail, run_gbc_lua

LUA = """
h.tap(h.KEY.A,    30)   -- Main menu -> Trainer
h.tap(h.KEY.DOWN, 60)   -- Name -> TID
h.tap(h.KEY.DOWN, 90)   -- TID -> Money
h.tap(h.KEY.DOWN, 120)  -- Money -> Badge 1
h.tap(h.KEY.B,    150)  -- Back to Main menu
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    orig, dumped, res, _ = run_gbc_lua(LUA, frames=200, fixture_name="rb.sav")
    if dumped != orig:
        fail("trainer_scene navigation mutated SRAM", res)
    print("OK: trainer_scene navigation left SRAM untouched")


if __name__ == "__main__":
    main()
