#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify stored PC Box slot 0 species edit round-trip across all 4 Gen I/II variants."""

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
        f"h.tap(h.KEY.A,     {fr + 30})",  # Enter MODE_EDIT on Species
        f"h.tap(h.KEY.RIGHT, {fr + 60})",  # Step species +1
        f"h.tap(h.KEY.A,     {fr + 90})",  # Exit MODE_EDIT
        f"h.tap(h.KEY.B,     {fr + 120})", # Back to Boxes
        f"h.tap(h.KEY.B,     {fr + 150})", # Back to Main menu (cursor reset to 0)
        f"h.tap(h.KEY.DOWN,  {fr + 180})",
        f"h.tap(h.KEY.DOWN,  {fr + 210})",
        f"h.tap(h.KEY.DOWN,  {fr + 240})",
        f"h.tap(h.KEY.DOWN,  {fr + 270})", # Save & Exit
        f"h.tap(h.KEY.A,     {fr + 300})", # Save
        'callbacks:add("shutdown", function() h.sram_dump("/dump/sram.bin") end)',
    ])
    return "\n".join(lines) + "\n", fr + 360


CASES = [
    ("rb.sav",      pk.Game.RB,      1, 0x9A),
    ("yellow.sav",  pk.Game.YELLOW,  1, 0x9A),
    ("gs.sav",      pk.Game.GS,      1, 0x9A),
    ("crystal.sav", pk.Game.CRYSTAL, 7, 0x9B),
]


def main():
    lib = load_lib()
    for fixture_name, expected_game, box_idx, expected_species in CASES:
        lua, frames = make_lua(box_idx)
        _, dumped, res, _ = run_gbc_lua(lua, frames=frames, fixture_name=fixture_name)
        game = pk.detect(dumped, lib=lib)
        save = pk.open(dumped, game=game, lib=lib)
        try:
            box, count = pk.get_box(save, box_idx)
        finally:
            pk.close(save)
        if count < 1 or box[0].species != expected_species:
            fail(
                f"{fixture_name}: expected box[{box_idx}][0].species == "
                f"0x{expected_species:02X}, got 0x{box[0].species:02X}",
                res,
            )
        if validate_checksum(lib, dumped, game) != 0:
            fail(f"{fixture_name}: checksum invalid after flush", res)
        print(
            f"  {fixture_name}: box[{box_idx}][0].species="
            f"0x{box[0].species:02X} OK"
        )
    print("OK: 4-variant box species round-trip passed")


if __name__ == "__main__":
    main()
