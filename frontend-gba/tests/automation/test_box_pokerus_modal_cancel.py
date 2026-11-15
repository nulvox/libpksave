#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""
Box-source Pokerus modal cancel test — opens Pokerus modal from box 1 slot 0
and verifies B cancels without writing and returns to pokemon_edit_scene
preserving the BOX edit source (so the next B returns to box_scene).

Run from the repo root:
    python3 frontend-gba/tests/automation/test_box_pokerus_modal_cancel.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_box_pokerus_modal_cancel.lua"
FRAMES = "780"
TIMEOUT_SECONDS = 180

FIXTURE_NAME = "rs.sav"
EXPECTED_GAME = pk.Game.RS
TARGET_BOX = 1
TARGET_SLOT = 0


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

    with tempfile.TemporaryDirectory(prefix="pksave-box-pkrs-cancel-") as workdir:
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
        if "box_pokerus_modal_cancel: OK" not in result.stdout:
            fail("'box_pokerus_modal_cancel: OK' marker not found", result)
        if "pokerus: OK" not in result.stdout:
            fail("'pokerus: OK' not found — modal did not construct from box source", result)
        if "pokerus: box=1 slot=0" not in result.stdout:
            fail("'pokerus: box=1 slot=0' not found — modal did not pick up box=1 slot=0", result)
        if "pokerus: cancel" not in result.stdout:
            fail("'pokerus: cancel' marker not found", result)

        dump_path = os.path.join(dump_dir, "sram.bin")
        with open(dump_path, "rb") as f:
            sram = f.read()
        if len(sram) != 131072:
            fail(f"expected 128 KiB FLASH dump, got {len(sram)}", result)

        lib = pk.load_library()
        with open(fixture_src, "rb") as f:
            fixture = f.read()
        baseline_save = pk.open(fixture, EXPECTED_GAME, lib=lib)
        try:
            baseline_box, _ = pk.get_box(baseline_save, TARGET_BOX)
            baseline_pkrs = pk.get_box_pokerus(baseline_save, TARGET_BOX, TARGET_SLOT)
        finally:
            pk.close(baseline_save)

        detected = pk.detect(sram, lib=lib)
        if detected != EXPECTED_GAME:
            fail(f"detect {detected.name} != {EXPECTED_GAME.name}", result)

        save = pk.open(sram, detected, lib=lib)
        try:
            box, _ = pk.get_box(save, TARGET_BOX)
            pkrs = pk.get_box_pokerus(save, TARGET_BOX, TARGET_SLOT)
        finally:
            pk.close(save)

        if box[TARGET_SLOT].species != baseline_box[TARGET_SLOT].species:
            fail(f"species mutated: {box[TARGET_SLOT].species} != "
                 f"{baseline_box[TARGET_SLOT].species}", result)
        if (pkrs.status != baseline_pkrs.status
                or pkrs.strain != baseline_pkrs.strain
                or pkrs.days != baseline_pkrs.days):
            fail("box pokerus mutated on cancel path", result)

    print("PASS: box pokerus modal open+cancel — box[1][0] pokerus and species unchanged")
    return 0


if __name__ == "__main__":
    sys.exit(main())
