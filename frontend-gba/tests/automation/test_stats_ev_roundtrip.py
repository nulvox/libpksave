#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Wave 2B test — STATS tab EV (HP) round-trip.

Drives: party → editor STATS tab → DOWN to ROW_EV_HP → A → RIGHT×3 (+30)
→ A commit → Save & Exit → dump.

Asserts:
- party[0].evs[0] (HP EV) = 30 (was 0 in fixture).
- party[0].species and level unchanged.
- 'edit: tab=1' marker fires (STATS tab transition).

The fixture seeds EVs to 0 across the board (per generate_fixtures.py:
Substruct 2 EVs are zeroed implicitly via bytearray(12)).

Run from the repo root:
    python3 frontend-gba/tests/automation/test_stats_ev_roundtrip.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_stats_ev_edit.lua"
FRAMES = "780"
TIMEOUT_SECONDS = 200

FIXTURE_NAME = "rs.sav"
EXPECTED_GAME = pk.Game.RS
INITIAL_HP_EV = 0
EXPECTED_HP_EV = 30
INITIAL_SPECIES = 1
INITIAL_LEVEL = 5


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

    with tempfile.TemporaryDirectory(prefix="pksave-stats-ev-") as workdir:
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
        if "stats_ev_edit: OK" not in result.stdout:
            fail("'stats_ev_edit: OK' marker not found", result)

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

        evs = [party[0].evs[i] for i in range(6)]
        print(f"  species: {party[0].species}")
        print(f"  level:   {party[0].level}")
        print(f"  evs:     {evs}")

        if party[0].species != INITIAL_SPECIES:
            fail(f"species mutated: {party[0].species}", result)
        if party[0].level != INITIAL_LEVEL:
            fail(f"level mutated: {party[0].level} != {INITIAL_LEVEL} — "
                 f"EV-only edit must not touch level", result)
        if party[0].evs[0] != EXPECTED_HP_EV:
            fail(f"HP EV {party[0].evs[0]} != expected {EXPECTED_HP_EV}; "
                 f"three RIGHT taps on ROW_EV_HP should produce +30", result)
        # Other EVs untouched.
        for i in range(1, 6):
            if party[0].evs[i] != 0:
                fail(f"EV[{i}] = {party[0].evs[i]} (expected 0); only HP "
                     f"EV should have changed", result)

    print(f"PASS: STATS EV(HP) round-trip — {INITIAL_HP_EV} → "
          f"{party[0].evs[0]} through FLASH; other EVs preserved")
    return 0


if __name__ == "__main__":
    sys.exit(main())
