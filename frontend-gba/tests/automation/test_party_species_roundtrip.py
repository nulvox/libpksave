#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
CP#3-C tracer-bullet test — round-trip a party-slot species edit through
the editor UI.

Loads rs.sav, drives the editor through main_menu → Party → A on slot 0
→ pokemon_edit_scene → RIGHT (species +1) → B → B → Save & Exit → FLASH
write → dump → ctypes decode → assert party[0].species == initial + 1.

Per libpksave/tests/fixtures/generate_fixtures.py, all three Gen-3
fixtures seed slot 0 with Bulbasaur (national dex 1) at level 5.  After
one RIGHT tap on the species cycler the value must be 2 (Ivysaur).

Trainer name and party count are preserved as negative controls: the
fixture's trainer "RED" and the single party slot remaining filled
prove the round-trip didn't corrupt unrelated fields.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_party_species_roundtrip.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_party_species_edit_save.lua"
FRAMES = "600"
TIMEOUT_SECONDS = 180

FIXTURE_NAME = "rs.sav"
EXPECTED_GAME = pk.Game.RS

# generate_fixtures.py seeds party slot 0 with Bulbasaur (Gen 3 national
# dex 1), level 5.  After one RIGHT tap on the species cycler the value
# must be initial + 1 = 2 (Ivysaur).
INITIAL_SPECIES = 1
EXPECTED_SPECIES = INITIAL_SPECIES + 1
EXPECTED_LEVEL   = 5
ORIGINAL_TRAINER_NAME = "RED"


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

    with tempfile.TemporaryDirectory(prefix="pksave-party-species-") as workdir:
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
        if "party_edit: OK" not in result.stdout:
            fail("'party_edit: OK' marker not found", result)
        if "party: OK" not in result.stdout:
            fail("'party: OK' marker not found — party_scene did not "
                 "construct (entry path regressed)", result)
        if "edit: OK" not in result.stdout:
            fail("'edit: OK' marker not found — pokemon_edit_scene did not "
                 "construct (party_scene's A handler did not transition or "
                 "the scene's ctor BN_LOG fired before the swap)", result)

        dump_path = os.path.join(dump_dir, "sram.bin")
        if not os.path.isfile(dump_path):
            fail(f"dump not produced: {dump_path}", result)

        with open(dump_path, "rb") as f:
            sram = f.read()
        if len(sram) != 131072:
            fail(f"expected 128 KiB FLASH dump, got {len(sram)}", result)

        lib = pk.load_library()
        detected = pk.detect(sram, lib=lib)
        if detected != EXPECTED_GAME:
            fail(f"detect {detected.name} != {EXPECTED_GAME.name} — "
                 f"slot 0 corrupted on save", result)

        save = pk.open(sram, detected, lib=lib)
        try:
            name = pk.get_trainer_name(save)
            party, count = pk.get_party(save)
        finally:
            pk.close(save)

        print(f"  trainer name:    {name!r}")
        print(f"  party count:     {count}")
        print(f"  slot 0 species:  {party[0].species} (was {INITIAL_SPECIES}, "
              f"expected {EXPECTED_SPECIES})")
        print(f"  slot 0 level:    {party[0].level}")

        if name != ORIGINAL_TRAINER_NAME:
            fail(f"trainer name {name!r} != {ORIGINAL_TRAINER_NAME!r} — "
                 f"species-edit drive script unexpectedly mutated the trainer "
                 f"block (or the fixture generator changed)", result)
        if count < 1:
            fail(f"party count {count} < 1 — slot 0 was emptied", result)
        if party[0].species == INITIAL_SPECIES:
            fail(f"species unchanged ({party[0].species}); cycler did not "
                 f"step or pokemon_edit_scene did not push to save / mark dirty",
                 result)
        if party[0].species != EXPECTED_SPECIES:
            fail(f"species {party[0].species} != expected {EXPECTED_SPECIES}; "
                 f"delta was {party[0].species - INITIAL_SPECIES}, "
                 f"expected +1.  A single 3-frame RIGHT tap on cycler should "
                 f"produce exactly one step.", result)
        if party[0].level != EXPECTED_LEVEL:
            fail(f"level {party[0].level} != expected {EXPECTED_LEVEL}; "
                 f"pksave_set_party_slot may have clobbered level (it should "
                 f"only touch fields the caller writes)", result)

    print(f"PASS: party-slot species round-trip — UI drove species "
          f"{INITIAL_SPECIES} → {EXPECTED_SPECIES} through FLASH; "
          f"trainer name preserved as {name!r}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
