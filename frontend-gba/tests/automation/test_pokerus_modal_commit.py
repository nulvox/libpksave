#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Wave 2B test — Pokerus modal commits a status cycle (ACTIVE → NEVER)
through FLASH.

Drive sequence: open modal → LEFT (cycle status one step backwards, which
takes the baseline ACTIVE → NEVER) → A (commit) → Save & Exit → dump.

Asserts:
- Post-run slot 0 pokerus status is NEVER, strain=0, days=0.
- Slot 0 species unchanged (Bulbasaur, 1).
- The "pokerus: commit" marker logged.

This proves the modal's status-cycler + commit path writes a pokerus
state distinct from the fixture baseline, which is the round-trip
the wave is gating on.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_pokerus_modal_commit.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_pokerus_modal_commit.lua"
FRAMES = "900"
TIMEOUT_SECONDS = 200

FIXTURE_NAME = "rs.sav"
EXPECTED_GAME = pk.Game.RS
EXPECTED_SPECIES = 1  # Bulbasaur


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

    with tempfile.TemporaryDirectory(prefix="pksave-pokerus-commit-") as workdir:
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
        if "pokerus_modal_commit: OK" not in result.stdout:
            fail("'pokerus_modal_commit: OK' marker not found", result)
        if "pokerus: OK" not in result.stdout:
            fail("'pokerus: OK' not found — modal did not construct", result)
        if "pokerus: commit" not in result.stdout:
            fail("'pokerus: commit' not found — A-press did not route "
                 "through the modal's commit path", result)

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
            pkrs = pk.get_party_pokerus(save, 0)
        finally:
            pk.close(save)

        print(f"  pokerus.status: {pk.PokerusStatus(pkrs.status).name}")
        print(f"  pokerus.strain: {pkrs.strain}")
        print(f"  pokerus.days:   {pkrs.days}")
        print(f"  species:        {party[0].species}")

        if party[0].species != EXPECTED_SPECIES:
            fail(f"species mutated: {party[0].species} != {EXPECTED_SPECIES}; "
                 f"pokerus commit should not have touched species", result)
        if pkrs.status != int(pk.PokerusStatus.NEVER):
            fail(f"pokerus status {pk.PokerusStatus(pkrs.status).name} != NEVER; "
                 f"LEFT-to-NEVER cycle then A-commit should have written NEVER",
                 result)
        if pkrs.strain != 0 or pkrs.days != 0:
            fail(f"pokerus nibbles strain={pkrs.strain} days={pkrs.days} != 0/0; "
                 f"NEVER must encode as strain=0/days=0", result)

    print("PASS: pokerus modal commit — ACTIVE → NEVER round-trip through FLASH")
    return 0


if __name__ == "__main__":
    sys.exit(main())
