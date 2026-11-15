#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""
CP#4 round-trip — edit a bag slot's quantity through the UI, save, and
verify the FLASH reflects the new quantity.

Loads rs.sav, drives main_menu → Bag → A on Items slot 0 (Potion ×5) →
DOWN to ROW_QTY → UP (+1) → A (commit) → B → Save & Exit → FLASH write
→ dump → ctypes decode → assert items[0] == (13, 6).

Negative controls (preserved fields prove the round-trip didn't corrupt
unrelated state):
  - trainer name still "RED"
  - items pocket count still >= 1
  - items[1] (the second seeded slot) unchanged

Run from the repo root:
    python3 frontend-gba/tests/automation/test_bag_slot_edit_roundtrip.py
"""

import os
import shutil
import subprocess
import sys
import tempfile

_HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", ".."))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)
import pksave_ctypes as pk  # noqa: E402

DOCKER_IMAGE = "pksave-test-gba"
EDITOR_ROM_CONTAINER = "/src/frontend-gba/pksave-gba.gba"
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_bag_slot_edit.lua"
FRAMES = "600"
TIMEOUT_SECONDS = 180

# Per-variant matrix (per libpksave/tests/fixtures/generate_fixtures.py):
#   RS / Emerald: Items slot 0 = Potion(13) ×5, slot 1 = item-0x44 ×2
#   FRLG:         Items slot 0 = Potion(13) ×7 (1 slot total)
# list_view consumes UP/DOWN; only LEFT/RIGHT survive on a numeric_field
# row, so the drive uses one RIGHT tap (= +10) instead of UP (= +1).
ORIGINAL_ITEM_ID = 13
QTY_DELTA        = 10
SECOND_SLOT_ITEM_ID = 0x44
SECOND_SLOT_QTY     = 2
ORIGINAL_TRAINER_NAME = "RED"

CASES = [
    ("rs.sav",      pk.Game.RS,      5),
    ("emerald.sav", pk.Game.EMERALD, 5),
    ("frlg.sav",    pk.Game.FRLG,    7),
]


def fail(msg, result=None):
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout (tail) ---\n{result.stdout[-2000:]}", file=sys.stderr)
        print(f"--- stderr (tail) ---\n{result.stderr[-1000:]}", file=sys.stderr)
    sys.exit(1)


def run_case(fixture_name, expected_game, original_qty, lib):
    expected_qty = original_qty + QTY_DELTA
    fixture_src = os.path.join(REPO_ROOT, "libpksave", "tests", "fixtures", fixture_name)
    if not os.path.isfile(fixture_src):
        fail(f"fixture missing: {fixture_src}")

    with tempfile.TemporaryDirectory(prefix=f"pksave-bag-edit-{fixture_name}-") as workdir:
        dump_dir = os.path.join(workdir, "dump")
        save_dir = os.path.join(workdir, "save")
        os.makedirs(dump_dir)
        os.makedirs(save_dir)

        save_path = os.path.join(save_dir, fixture_name)
        shutil.copy2(fixture_src, save_path)

        try:
            result = subprocess.run(
                ["docker", "run", "--rm",
                 "-v", f"{REPO_ROOT}:/src:ro",
                 "-v", f"{save_dir}:/save:rw",
                 "-v", f"{dump_dir}:/dump:rw",
                 DOCKER_IMAGE,
                 "mgba-lua",
                 EDITOR_ROM_CONTAINER, DRIVE_LUA_CONTAINER, FRAMES,
                 "--save", f"/save/{fixture_name}"],
                capture_output=True, text=True, timeout=TIMEOUT_SECONDS,
            )
        except FileNotFoundError:
            fail("docker not found on PATH")
        except subprocess.TimeoutExpired:
            fail(f"{fixture_name}: mgba-lua did not exit within {TIMEOUT_SECONDS}s")

        if result.returncode != 0:
            fail(f"{fixture_name}: mgba-lua exited {result.returncode}", result)
        if "bag_edit: OK" not in result.stdout:
            fail(f"{fixture_name}: 'bag_edit: OK' marker not found", result)
        if "bag: OK" not in result.stdout:
            fail(f"{fixture_name}: 'bag: OK' marker not found — bag_scene did not "
                 f"construct (main menu wiring or scene_kind::BAG dispatch "
                 f"regressed)", result)
        if "slot_edit: OK" not in result.stdout:
            fail(f"{fixture_name}: 'slot_edit: OK' marker not found — slot_edit_scene did "
                 f"not construct (bag_scene's A handler did not transition, "
                 f"or scene_kind::SLOT_EDIT dispatch is broken)", result)

        dump_path = os.path.join(dump_dir, "sram.bin")
        if not os.path.isfile(dump_path):
            fail(f"{fixture_name}: dump not produced: {dump_path}", result)

        with open(dump_path, "rb") as f:
            sram = f.read()
        if len(sram) != 131072:
            fail(f"{fixture_name}: expected 128 KiB FLASH dump, got {len(sram)}", result)

        detected = pk.detect(sram, lib=lib)
        if detected != expected_game:
            fail(f"{fixture_name}: detect {detected.name} != {expected_game.name} — "
                 f"slot 0 corrupted on save", result)

        save = pk.open(sram, detected, lib=lib)
        try:
            name = pk.get_trainer_name(save)
            items, items_count = pk.get_pocket(save, pk.Pocket.ITEMS)
        finally:
            pk.close(save)

        if name != ORIGINAL_TRAINER_NAME:
            fail(f"{fixture_name}: trainer name {name!r} != {ORIGINAL_TRAINER_NAME!r} — "
                 f"bag-edit drive script unexpectedly mutated the trainer "
                 f"block (or the fixture generator changed)", result)
        if items_count < 1:
            fail(f"{fixture_name}: items pocket count {items_count} < 1 — slot 0 was emptied",
                 result)
        if items[0].item_id != ORIGINAL_ITEM_ID:
            fail(f"{fixture_name}: items[0].item_id {items[0].item_id} != "
                 f"{ORIGINAL_ITEM_ID} — slot's item id changed; expected "
                 f"only the qty to move (cycler stepped unexpectedly?)", result)
        if items[0].quantity == original_qty:
            fail(f"{fixture_name}: items[0].quantity unchanged ({items[0].quantity}); "
                 f"slot_edit_scene did not increment qty or did not commit",
                 result)
        if items[0].quantity != expected_qty:
            fail(f"{fixture_name}: items[0].quantity {items[0].quantity} != expected "
                 f"{expected_qty}; delta was {items[0].quantity - original_qty}, "
                 f"expected +10.  One 3-frame RIGHT tap on the qty field "
                 f"should produce exactly one +10 step.", result)
        if items_count >= 2:
            if items[1].item_id != SECOND_SLOT_ITEM_ID \
                    or items[1].quantity != SECOND_SLOT_QTY:
                fail(f"{fixture_name}: items[1] = ({items[1].item_id}, {items[1].quantity}) "
                     f"!= expected ({SECOND_SLOT_ITEM_ID}, "
                     f"{SECOND_SLOT_QTY}) — adjacent slot mutated; the "
                     f"edit was too broad", result)

        print(f"  OK  {fixture_name:12s} detect={detected.name:7s} "
              f"Items[0].qty {original_qty} → {items[0].quantity}")


def main():
    lib = pk.load_library()
    print(f"running {len(CASES)} Gen-3 bag slot-edit round-trip cases:")
    for fixture_name, expected_game, original_qty in CASES:
        run_case(fixture_name, expected_game, original_qty, lib)
    print(f"PASS: {len(CASES)}/{len(CASES)} variants edited Items[0].qty (+10) through FLASH")
    return 0


if __name__ == "__main__":
    sys.exit(main())
