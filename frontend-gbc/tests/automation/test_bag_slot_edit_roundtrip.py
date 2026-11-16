#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify Bag Items[0].quantity +10 round-trip across all 4 Gen I/II variants."""

from gbc_harness import fail, load_lib, pk, run_gbc_lua, validate_checksum

LUA = """
h.tap(h.KEY.DOWN,  30)   -- Main menu -> Party
h.tap(h.KEY.DOWN,  60)   -- Party -> Bag
h.tap(h.KEY.A,     90)   -- Enter Bag (ITEMS pocket, slot 0)
h.tap(h.KEY.A,     120)  -- Enter slot_edit_scene (ROW_ITEM = 0)
h.tap(h.KEY.DOWN,  150)  -- ROW_ITEM -> ROW_QTY
h.tap(h.KEY.RIGHT, 180)  -- +10 qty
h.tap(h.KEY.A,     210)  -- Commit slot
h.tap(h.KEY.B,     240)  -- Back to Main menu (cursor reset to 0)
h.tap(h.KEY.DOWN,  270)
h.tap(h.KEY.DOWN,  300)
h.tap(h.KEY.DOWN,  330)
h.tap(h.KEY.DOWN,  360)  -- Save & Exit
h.tap(h.KEY.A,     390)  -- Save
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""

CASES = [
    ("rb.sav",      pk.Game.RB,      0x01, 1 + 10),
    ("yellow.sav",  pk.Game.YELLOW,  0x01, 1 + 10),
    ("gs.sav",      pk.Game.GS,      0x12, 5 + 10),
    ("crystal.sav", pk.Game.CRYSTAL, 0x12, 3 + 10),
]


def main():
    lib = load_lib()
    for fixture_name, expected_game, expected_id, expected_qty in CASES:
        _, dumped, res, _ = run_gbc_lua(LUA, frames=460, fixture_name=fixture_name)
        game = pk.detect(dumped, lib=lib)
        save = pk.open(dumped, game=game, lib=lib)
        try:
            items, count = pk.get_pocket(save, pk.Pocket.ITEMS)
        finally:
            pk.close(save)
        if count < 1:
            fail(f"{fixture_name}: expected ITEMS count >= 1, got {count}", res)
        if items[0].item_id != expected_id or items[0].quantity != expected_qty:
            fail(
                f"{fixture_name}: expected item=0x{expected_id:02X} qty={expected_qty}, "
                f"got item=0x{items[0].item_id:02X} qty={items[0].quantity}",
                res,
            )
        if validate_checksum(lib, dumped, game) != 0:
            fail(f"{fixture_name}: checksum invalid after flush", res)
        print(
            f"  {fixture_name}: item=0x{items[0].item_id:02X} "
            f"qty={items[0].quantity} OK"
        )
    print("OK: 4-variant bag slot edit round-trip passed")


if __name__ == "__main__":
    main()
