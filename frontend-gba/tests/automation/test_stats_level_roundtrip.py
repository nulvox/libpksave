#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Wave 2B test — STATS tab level round-trip.

Drives: party → editor → STATS tab → A on ROW_LEVEL → RIGHT×3 (+30 via
numeric_field's ±10 grammar) → A commit → Save & Exit → dump.

Asserts:
- party[0].level == 35 (was 5; +30 expected).
- party[0].species unchanged (1, Bulbasaur).
- "edit: tab=1" log marker fires.
- No emulator timeout / crash on STATS tab construction (13 rows).

Run from the repo root:
    python3 frontend-gba/tests/automation/test_stats_level_roundtrip.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_stats_level_edit.lua"
FRAMES = "780"
TIMEOUT_SECONDS = 200

FIXTURE_NAME = "rs.sav"
EXPECTED_GAME = pk.Game.RS
INITIAL_LEVEL = 5
EXPECTED_LEVEL = 35  # 5 + 3*10
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

    with tempfile.TemporaryDirectory(prefix="pksave-stats-level-") as workdir:
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
        if "stats_level_edit: OK" not in result.stdout:
            fail("'stats_level_edit: OK' marker not found", result)
        if "edit: tab=1" not in result.stdout:
            fail("'edit: tab=1' marker not found — STATS tab transition "
                 "did not log", result)

        dump_path = os.path.join(dump_dir, "sram.bin")
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
            party, count = pk.get_party(save)
        finally:
            pk.close(save)

        print(f"  party count:     {count}")
        print(f"  slot 0 species:  {party[0].species}")
        print(f"  slot 0 level:    {party[0].level} (was {INITIAL_LEVEL}, "
              f"expected {EXPECTED_LEVEL})")

        if party[0].species != INITIAL_SPECIES:
            fail(f"species mutated: {party[0].species} != {INITIAL_SPECIES}; "
                 f"STATS-level edit should not touch species", result)
        if party[0].level == INITIAL_LEVEL:
            fail(f"level unchanged ({party[0].level}); numeric_field "
                 f"RIGHT taps did not step or commit_stats_row did not "
                 f"push to save", result)
        if party[0].level != EXPECTED_LEVEL:
            fail(f"level {party[0].level} != expected {EXPECTED_LEVEL}; "
                 f"delta was {party[0].level - INITIAL_LEVEL}, expected "
                 f"+30 from three RIGHT taps", result)

    print(f"PASS: STATS level round-trip — {INITIAL_LEVEL} → "
          f"{party[0].level} through FLASH; species preserved")
    return 0


if __name__ == "__main__":
    sys.exit(main())
