#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify stored PC Box slot 0 level edit (+30) across all 4 Gen I/II variants."""

from gbc_harness import fail, load_lib, pk, run_gbc_lua, validate_checksum


def make_lua(select_taps: int) -> tuple[str, int]:
    lines = [
        "h.tap(h.KEY.DOWN, 30)",
        "h.tap(h.KEY.DOWN, 60)",
        "h.tap(h.KEY.DOWN, 90)",
        "h.tap(h.KEY.A,    120)",
    ]
    fr = 150
    for _ in range(select_taps):
        lines.append(f"h.tap(h.KEY.SELECT, {fr})")
        fr += 30
    lines.extend([
        f"h.tap(h.KEY.A,     {fr})",       # Enter Pokemon Edit
        f"h.tap(h.KEY.RIGHT, {fr + 30})",  # STATS tab (ROW_LEVEL = 0)
        f"h.tap(h.KEY.A,     {fr + 60})",  # Enter MODE_EDIT on Level
        f"h.tap(h.KEY.RIGHT, {fr + 90})",  # +10
        f"h.tap(h.KEY.RIGHT, {fr + 120})", # +10
        f"h.tap(h.KEY.RIGHT, {fr + 150})", # +10
        f"h.tap(h.KEY.A,     {fr + 180})", # Exit MODE_EDIT
        f"h.tap(h.KEY.B,     {fr + 210})", # Back to Boxes
        f"h.tap(h.KEY.B,     {fr + 240})", # Back to Main menu (cursor reset to 0)
        f"h.tap(h.KEY.DOWN,  {fr + 270})",
        f"h.tap(h.KEY.DOWN,  {fr + 300})",
        f"h.tap(h.KEY.DOWN,  {fr + 330})",
        f"h.tap(h.KEY.DOWN,  {fr + 360})", # Save & Exit
        f"h.tap(h.KEY.A,     {fr + 390})", # Save
        'callbacks:add("shutdown", function() h.sram_dump("/dump/sram.bin") end)',
    ])
    return "\n".join(lines) + "\n", fr + 450


CASES = [
    ("rb.sav",      pk.Game.RB,      1, 7 + 30),
    ("yellow.sav",  pk.Game.YELLOW,  1, 7 + 30),
    ("gs.sav",      pk.Game.GS,      1, 8 + 30),
    ("crystal.sav", pk.Game.CRYSTAL, 7, 16 + 30),
]


def main():
    lib = load_lib()
    for fixture_name, expected_game, box_idx, expected_level in CASES:
        lua, frames = make_lua(box_idx)
        _, dumped, res, _ = run_gbc_lua(lua, frames=frames, fixture_name=fixture_name)
        game = pk.detect(dumped, lib=lib)
        save = pk.open(dumped, game=game, lib=lib)
        try:
            box, count = pk.get_box(save, box_idx)
        finally:
            pk.close(save)
        if count < 1 or box[0].level != expected_level:
            fail(
                f"{fixture_name}: expected box[{box_idx}][0].level == "
                f"{expected_level}, got {box[0].level}",
                res,
            )
        if validate_checksum(lib, dumped, game) != 0:
            fail(f"{fixture_name}: checksum invalid after flush", res)
        print(
            f"  {fixture_name}: box[{box_idx}][0].level={box[0].level} OK"
        )
    print("OK: 4-variant box stats level round-trip passed")


if __name__ == "__main__":
    main()
