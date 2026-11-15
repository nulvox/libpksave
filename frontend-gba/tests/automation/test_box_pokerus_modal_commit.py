#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""
Box-source Pokerus modal commit test — commits a status cycle (ACTIVE → NEVER)
on box 1 slot 0 through FLASH, verifying that party[0]'s Pokerus remains
untouched (ACTIVE) as a negative control.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_box_pokerus_modal_commit.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_box_pokerus_modal_commit.lua"
FRAMES = "1000"
TIMEOUT_SECONDS = 200

FIXTURE_NAME = "rs.sav"
EXPECTED_GAME = pk.Game.RS
TARGET_BOX = 1
TARGET_SLOT = 0
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

    with tempfile.TemporaryDirectory(prefix="pksave-box-pkrs-commit-") as workdir:
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
        if "box_pokerus_modal_commit: OK" not in result.stdout:
            fail("'box_pokerus_modal_commit: OK' marker not found", result)
        if "pokerus: OK" not in result.stdout:
            fail("'pokerus: OK' not found — modal did not construct", result)
        if "pokerus: commit box=1 slot=0" not in result.stdout:
            fail("'pokerus: commit box=1 slot=0' not found — commit did not route "
                 "through box 1 slot 0", result)

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
            box, _ = pk.get_box(save, TARGET_BOX)
            box_pkrs = pk.get_box_pokerus(save, TARGET_BOX, TARGET_SLOT)
            party_pkrs = pk.get_party_pokerus(save, 0)
        finally:
            pk.close(save)

        print(f"  box[1][0] pokerus.status: {pk.PokerusStatus(box_pkrs.status).name}")
        print(f"  box[1][0] pokerus.strain: {box_pkrs.strain}")
        print(f"  box[1][0] pokerus.days:   {box_pkrs.days}")
        print(f"  box[1][0] species:        {box[TARGET_SLOT].species}")
        print(f"  party[0]  pokerus.status: {pk.PokerusStatus(party_pkrs.status).name}")

        if box[TARGET_SLOT].species != EXPECTED_SPECIES:
            fail(f"box species mutated: {box[TARGET_SLOT].species} != {EXPECTED_SPECIES}", result)
        if box_pkrs.status != int(pk.PokerusStatus.NEVER):
            fail(f"box pokerus status {pk.PokerusStatus(box_pkrs.status).name} != NEVER", result)
        if box_pkrs.strain != 0 or box_pkrs.days != 0:
            fail(f"box pokerus nibbles strain={box_pkrs.strain} days={box_pkrs.days} != 0/0", result)
        if party_pkrs.status != int(pk.PokerusStatus.ACTIVE):
            fail(f"party[0] pokerus was mutated ({pk.PokerusStatus(party_pkrs.status).name}); "
                 f"box pokerus commit leaked into party slot 0", result)

    print("PASS: box pokerus modal commit — box[1][0] ACTIVE → NEVER round-trip "
          "through FLASH; party[0] untouched")
    return 0


if __name__ == "__main__":
    sys.exit(main())
