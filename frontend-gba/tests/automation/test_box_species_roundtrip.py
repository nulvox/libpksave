#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""
CP#5 tracer-bullet — round-trip a box-slot species edit through the editor UI.

Loads rs.sav (fixture seeds box 1 slot 0 with Bulbasaur / level 5), drives
the editor through main_menu → Boxes → SELECT (box 0→1) → A on slot 0 →
pokemon_edit_scene → A (enter ACTIVE on ROW_SPECIES) → RIGHT (species +1) →
A (commit) → B → B → Save & Exit → FLASH write → dump → ctypes decode →
assert box[1][0].species == 2 (Ivysaur).

Negative controls (catch off-by-one bugs in the box-index handoff or a
mistaken write to party storage):
  - box[0]      must remain empty (off-by-one would land here)
  - party[0].species must remain Bulbasaur (1) — would catch a wire that
    routes box-edit through pksave_set_party_slot
  - trainer name 'RED' preserved (catches whole-save corruption)

Run from the repo root:
    python3 frontend-gba/tests/automation/test_box_species_roundtrip.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_box_species_edit_save.lua"
FRAMES = "600"
TIMEOUT_SECONDS = 180

# generate_fixtures.py seeds box 1 slot 0 with Bulbasaur (Gen 3 national
# dex 1) at level 5 across all three Gen 3 variants.  After one RIGHT tap
# on the species cycler the value must be initial + 1 = 2 (Ivysaur).
EDITED_BOX = 1
EDITED_SLOT = 0
INITIAL_SPECIES  = 1
EXPECTED_SPECIES = INITIAL_SPECIES + 1
ORIGINAL_TRAINER_NAME = "RED"

CASES = [
    ("rs.sav",      pk.Game.RS),
    ("emerald.sav", pk.Game.EMERALD),
    ("frlg.sav",    pk.Game.FRLG),
]


def fail(msg, result=None):
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout (tail) ---\n{result.stdout[-2000:]}", file=sys.stderr)
        print(f"--- stderr (tail) ---\n{result.stderr[-1000:]}", file=sys.stderr)
    sys.exit(1)


def run_case(fixture_name, expected_game, lib):
    fixture_src = os.path.join(REPO_ROOT, "libpksave", "tests", "fixtures", fixture_name)
    if not os.path.isfile(fixture_src):
        fail(f"fixture missing: {fixture_src}")

    with tempfile.TemporaryDirectory(prefix=f"pksave-box-species-{fixture_name}-") as workdir:
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
        if "box_edit: OK" not in result.stdout:
            fail(f"{fixture_name}: 'box_edit: OK' marker not found (drive script did not "
                 f"reach shutdown)", result)
        if "box: OK" not in result.stdout:
            fail(f"{fixture_name}: 'box: OK' marker not found — box_scene did not "
                 f"construct (BOXES menu entry did not transition; "
                 f"main_menu_scene's switch may still be no-op'ing BOXES)",
                 result)
        if "edit: OK" not in result.stdout:
            fail(f"{fixture_name}: 'edit: OK' marker not found — pokemon_edit_scene did not "
                 f"construct from box_scene (box_scene's A handler did not "
                 f"transition, or its pending_box_* handoff is unwired)",
                 result)

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
                 f"box-edit corrupted the save", result)

        save = pk.open(sram, detected, lib=lib)
        try:
            name              = pk.get_trainer_name(save)
            party, _party_cnt = pk.get_party(save)
            box0, _box0_cnt   = pk.get_box(save, 0)
            box1, _box1_cnt   = pk.get_box(save, EDITED_BOX)
        finally:
            pk.close(save)

        # Filled-slot counts (true population), derived from species == 0:
        box0_filled = sum(1 for s in box0 if s.species != 0)
        box1_filled = sum(1 for s in box1 if s.species != 0)

        # Primary assertion — the box-edit landed.
        if box1[EDITED_SLOT].species == INITIAL_SPECIES:
            fail(f"{fixture_name}: box[{EDITED_BOX}][{EDITED_SLOT}].species unchanged "
                 f"({box1[EDITED_SLOT].species}); cycler did not step OR "
                 f"pokemon_edit_scene did not push to save / mark dirty OR "
                 f"box_scene did not record the right pending_box_* pair",
                 result)
        if box1[EDITED_SLOT].species != EXPECTED_SPECIES:
            fail(f"{fixture_name}: box[{EDITED_BOX}][{EDITED_SLOT}].species "
                 f"{box1[EDITED_SLOT].species} != expected {EXPECTED_SPECIES}; "
                 f"a single 3-frame RIGHT tap on cycler should produce "
                 f"exactly one step", result)

        # Negative controls — make sure the edit landed in the right place.
        if box0_filled != 0:
            fail(f"{fixture_name}: box[0] has {box0_filled} filled slots (expected 0) — "
                 f"edit landed in the wrong box (off-by-one in "
                 f"pending_box_index, or SELECT didn't cycle the box "
                 f"pointer; fixture seeds nothing in box 0)", result)
        if box1_filled != 1:
            fail(f"{fixture_name}: box[{EDITED_BOX}] has {box1_filled} filled slots "
                 f"(expected 1) — pksave_set_box_slot wrote into slots "
                 f"other than the requested one, or the edit script "
                 f"clobbered an adjacent slot", result)
        if party[0].species != INITIAL_SPECIES:
            fail(f"{fixture_name}: party[0].species {party[0].species} != {INITIAL_SPECIES} — "
                 f"box-edit was routed through pksave_set_party_slot instead "
                 f"of pksave_set_box_slot (source-branch in pokemon_edit_scene "
                 f"is wrong)", result)
        if name != ORIGINAL_TRAINER_NAME:
            fail(f"{fixture_name}: trainer name {name!r} != {ORIGINAL_TRAINER_NAME!r} — "
                 f"box-edit drive script unexpectedly mutated the trainer "
                 f"block (whole-save corruption)", result)

        print(f"  OK  {fixture_name:12s} detect={detected.name:7s} "
              f"box[{EDITED_BOX}][{EDITED_SLOT}].species "
              f"{INITIAL_SPECIES} → {box1[EDITED_SLOT].species}")


def main():
    lib = pk.load_library()
    print(f"running {len(CASES)} Gen-3 box-slot species round-trip cases:")
    for fixture_name, expected_game in CASES:
        run_case(fixture_name, expected_game, lib)
    print(f"PASS: {len(CASES)}/{len(CASES)} variants edited "
          f"box[{EDITED_BOX}][{EDITED_SLOT}].species "
          f"{INITIAL_SPECIES} → {EXPECTED_SPECIES} through FLASH")
    return 0


if __name__ == "__main__":
    sys.exit(main())
