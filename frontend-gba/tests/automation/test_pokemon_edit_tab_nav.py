#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Wave 2A test 1 — pokemon_edit_scene tab cycling in NAV mode.

Drives:
    main_menu → DOWN → A → party → A → editor
        RIGHT → tab IDENTITY → STATS
        RIGHT → tab STATS    → MOVES
        RIGHT → tab MOVES    → IDENTITY (wrap)
        LEFT  → tab IDENTITY → MOVES   (wrap)
    B → party → B → main_menu → Save & Exit

Asserts:
    - stdout has 'edit: tab=1', 'edit: tab=2', 'edit: tab=0', 'edit: tab=2'
      in that order (each tab change logs the new tab index).
    - slot 0 species is still 1 (Bulbasaur), proving NAV-mode L/R did
      NOT cycle the species cycler — the active-edit gate is in place.

This is the round-trip companion to the active-edit gate: tab cycling
must not bleed into the focused field.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_pokemon_edit_tab_nav.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_pokemon_edit_tab_nav.lua"
FRAMES = "600"
TIMEOUT_SECONDS = 180

FIXTURE_NAME = "rs.sav"
EXPECTED_GAME = pk.Game.RS
INITIAL_SPECIES = 1
ORIGINAL_TRAINER_NAME = "RED"


def fail(msg, result=None):
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout (tail) ---\n{result.stdout[-2000:]}", file=sys.stderr)
        print(f"--- stderr (tail) ---\n{result.stderr[-1000:]}", file=sys.stderr)
    sys.exit(1)


def find_ordered(haystack, needles):
    """Return True iff every needle appears in haystack with at least the
    given order (not necessarily contiguous, not necessarily unique)."""
    cursor = 0
    for n in needles:
        idx = haystack.find(n, cursor)
        if idx < 0:
            return False, n
        cursor = idx + len(n)
    return True, None


def main():
    fixture_src = os.path.join(REPO_ROOT, "libpksave", "tests", "fixtures", FIXTURE_NAME)
    if not os.path.isfile(fixture_src):
        fail(f"fixture missing: {fixture_src}")

    with tempfile.TemporaryDirectory(prefix="pksave-tab-nav-") as workdir:
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
        if "tab_nav: OK" not in result.stdout:
            fail("'tab_nav: OK' marker not found", result)
        if "edit: OK" not in result.stdout:
            fail("'edit: OK' not found — pokemon_edit_scene did not construct", result)

        ok, missing = find_ordered(
            result.stdout,
            ["edit: tab=1", "edit: tab=2", "edit: tab=0", "edit: tab=2"],
        )
        if not ok:
            fail(f"expected ordered tab markers; missing {missing!r}", result)

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
            fail(f"detect {detected.name} != {EXPECTED_GAME.name}", result)

        save = pk.open(sram, detected, lib=lib)
        try:
            name = pk.get_trainer_name(save)
            party, count = pk.get_party(save)
        finally:
            pk.close(save)

        if name != ORIGINAL_TRAINER_NAME:
            fail(f"trainer name mutated: {name!r} != {ORIGINAL_TRAINER_NAME!r}", result)
        if count < 1:
            fail("party emptied", result)
        if party[0].species != INITIAL_SPECIES:
            fail(f"species changed to {party[0].species}; NAV-mode L/R "
                 f"should have cycled tabs, NOT the species cycler. The "
                 f"active-edit gate is missing or broken.", result)

    print(f"PASS: pokemon_edit tab nav — IDENTITY/STATS/MOVES cycle; "
          f"species preserved as {INITIAL_SPECIES} (Bulbasaur)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
