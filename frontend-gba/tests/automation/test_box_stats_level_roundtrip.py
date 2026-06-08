#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
CP#5 + L11 — round-trip a box-resident STATS-tab level edit.

Loads rs.sav (fixture seeds box 1 slot 0 with Bulbasaur level 5), drives
main_menu → Boxes → SELECT (0→1) → A → pokemon_edit_scene → RIGHT (tab to
STATS) → A (ACTIVE on ROW_LEVEL) → RIGHT×3 (+30) → A (commit) → B → B →
Save & Exit.  Asserts box[1][0].level == 35 and the rest of the fixture is
intact.

This is the harness counterpart to test_rs_set_box_slot_level_round_trip
(Unity).  Together they prove L11: the UI can change a box mon's level
and the change survives a real FLASH round-trip through the encoder.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_box_stats_level_roundtrip.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_box_stats_level_edit.lua"
FRAMES = "900"
TIMEOUT_SECONDS = 240

FIXTURE_NAME = "rs.sav"
EXPECTED_GAME = pk.Game.RS

EDITED_BOX  = 1
EDITED_SLOT = 0
INITIAL_LEVEL  = 5
EXPECTED_LEVEL = INITIAL_LEVEL + 30   # 3 × RIGHT = 3 × +10 on numeric_field
ORIGINAL_TRAINER_NAME = "RED"
INITIAL_SPECIES = 1


def fail(msg, result=None):
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout (tail) ---\n{result.stdout[-2000:]}", file=sys.stderr)
        print(f"--- stderr (tail) ---\n{result.stderr[-1000:]}", file=sys.stderr)
    sys.exit(1)


def main():
    fixture_src = os.path.join(REPO_ROOT, "libpksave", "tests", "fixtures", FIXTURE_NAME)
    if not os.path.isfile(fixture_src):
        fail(f"fixture missing: {fixture_src}")

    with tempfile.TemporaryDirectory(prefix="pksave-box-level-") as workdir:
        dump_dir = os.path.join(workdir, "dump")
        save_dir = os.path.join(workdir, "save")
        os.makedirs(dump_dir)
        os.makedirs(save_dir)

        save_path = os.path.join(save_dir, FIXTURE_NAME)
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
                 "--save", f"/save/{FIXTURE_NAME}"],
                capture_output=True, text=True, timeout=TIMEOUT_SECONDS,
            )
        except FileNotFoundError:
            fail("docker not found on PATH")
        except subprocess.TimeoutExpired:
            fail(f"mgba-lua did not exit within {TIMEOUT_SECONDS}s")

        if result.returncode != 0:
            fail(f"mgba-lua exited {result.returncode}", result)
        if "box_level_edit: OK" not in result.stdout:
            fail("'box_level_edit: OK' marker not found (drive script "
                 "did not reach shutdown)", result)
        if "box: OK" not in result.stdout:
            fail("'box: OK' marker not found — box_scene did not "
                 "construct", result)
        if "edit: OK" not in result.stdout:
            fail("'edit: OK' marker not found — pokemon_edit_scene did "
                 "not construct from box_scene", result)

        dump_path = os.path.join(dump_dir, "sram.bin")
        with open(dump_path, "rb") as f:
            sram = f.read()
        if len(sram) != 131072:
            fail(f"expected 128 KiB FLASH dump, got {len(sram)}", result)

        lib = pk.load_library()
        detected = pk.detect(sram, lib=lib)
        if detected != EXPECTED_GAME:
            fail(f"detect {detected.name} != {EXPECTED_GAME.name} — "
                 f"box-edit corrupted the save", result)

        save = pk.open(sram, detected, lib=lib)
        try:
            name             = pk.get_trainer_name(save)
            box1, _          = pk.get_box(save, EDITED_BOX)
            party, party_cnt = pk.get_party(save)
        finally:
            pk.close(save)

        print(f"  trainer name:                       {name!r}")
        print(f"  party[0].species:                   {party[0].species}")
        print(f"  box[{EDITED_BOX}][{EDITED_SLOT}].species: "
              f"{box1[EDITED_SLOT].species}")
        print(f"  box[{EDITED_BOX}][{EDITED_SLOT}].level:   "
              f"{box1[EDITED_SLOT].level} (was {INITIAL_LEVEL}, "
              f"expected {EXPECTED_LEVEL})")

        if box1[EDITED_SLOT].level == INITIAL_LEVEL:
            fail(f"box[{EDITED_BOX}][{EDITED_SLOT}].level unchanged at "
                 f"{INITIAL_LEVEL} — L11 regression: encoder did not "
                 f"persist the level change to the Growth substruct's XP "
                 f"field, or g3_level_from_xp didn't round-trip", result)
        if box1[EDITED_SLOT].level != EXPECTED_LEVEL:
            fail(f"box[{EDITED_BOX}][{EDITED_SLOT}].level "
                 f"{box1[EDITED_SLOT].level} != expected {EXPECTED_LEVEL}; "
                 f"3 × RIGHT taps on numeric_field should produce 3 × +10 "
                 f"= +30; if delta < 30, butano debounce may need a "
                 f"longer inter-tap gap", result)
        if box1[EDITED_SLOT].species != INITIAL_SPECIES:
            fail(f"box[{EDITED_BOX}][{EDITED_SLOT}].species changed "
                 f"({box1[EDITED_SLOT].species} != {INITIAL_SPECIES}); "
                 f"level edit must not mutate species", result)
        if party[0].species != INITIAL_SPECIES:
            fail(f"party[0].species {party[0].species} != {INITIAL_SPECIES} "
                 f"— level edit on box source corrupted party storage", result)
        if name != ORIGINAL_TRAINER_NAME:
            fail(f"trainer name {name!r} != {ORIGINAL_TRAINER_NAME!r} — "
                 f"box-edit drive script unexpectedly mutated trainer block",
                 result)

    print(f"PASS: box-slot STATS level round-trip — UI drove "
          f"box[{EDITED_BOX}][{EDITED_SLOT}].level "
          f"{INITIAL_LEVEL} → {EXPECTED_LEVEL} through FLASH; "
          f"species + party + trainer preserved")
    return 0


if __name__ == "__main__":
    sys.exit(main())
