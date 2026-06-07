#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Wave 2C test — MOVES tab PP round-trip.

Drives: party → editor → LEFT (→MOVES) → DOWN to ROW_PP_0 → A → LEFT
(-10 on numeric_field) → A commit → Save & Exit.

Fixture: party[0].move_ids[0] = 33 (Tackle), move_pp[0] = 35.
Tackle base PP 35 → cap = floor(35 * 1.6) = 56.  A -10 step lands PP
at 25, well below the cap so no clamping.

Asserts:
- party[0].move_pp[0] == 25.
- party[0].move_ids[0] == 33 (unchanged — PP-only edit).
- 'edit: tab=2' marker fires.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_moves_pp_roundtrip.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_moves_pp_edit.lua"
FRAMES = "760"
TIMEOUT_SECONDS = 200

FIXTURE_NAME = "rs.sav"
EXPECTED_GAME = pk.Game.RS
INITIAL_SPECIES = 1
INITIAL_LEVEL = 5
INITIAL_MOVE_0 = 33
INITIAL_PP_0 = 35
EXPECTED_PP_0 = 25


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

    with tempfile.TemporaryDirectory(prefix="pksave-moves-pp-") as workdir:
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
        if "moves_pp_edit: OK" not in result.stdout:
            fail("'moves_pp_edit: OK' marker not found", result)
        if "edit: tab=2" not in result.stdout:
            fail("'edit: tab=2' marker not found — MOVES tab transition "
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

        move_ids = [party[0].move_ids[i] for i in range(4)]
        move_pp  = [party[0].move_pp[i]  for i in range(4)]
        print(f"  species:   {party[0].species}")
        print(f"  level:     {party[0].level}")
        print(f"  move_ids:  {move_ids}")
        print(f"  move_pp:   {move_pp}")

        if party[0].species != INITIAL_SPECIES:
            fail(f"species mutated: {party[0].species}", result)
        if party[0].level != INITIAL_LEVEL:
            fail(f"level mutated: {party[0].level}", result)
        if move_ids[0] != INITIAL_MOVE_0:
            fail(f"move_ids[0] {move_ids[0]} != {INITIAL_MOVE_0}; "
                 f"PP-only edit should not touch the move id", result)
        if move_pp[0] != EXPECTED_PP_0:
            fail(f"move_pp[0] {move_pp[0]} != expected {EXPECTED_PP_0}; "
                 f"LEFT tap on ROW_PP_0 should step PP {INITIAL_PP_0} "
                 f"→ {EXPECTED_PP_0} (-10)", result)

    print(f"PASS: MOVES PP round-trip — slot 0 PP {INITIAL_PP_0}→"
          f"{move_pp[0]} (move_id unchanged at {move_ids[0]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
