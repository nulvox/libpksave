#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Navigation no-mutation check for the Party scene (CP#3-B).

Drives: main_menu → DOWN → A → party_scene → DOWN × 2 → B → main_menu.

Loads rs.sav as a sidecar fixture so the editor reaches main_menu (with
no save, pksave_detect returns UNKNOWN and the editor halts on
boot_error_scene instead — which would silently mask any party-scene
regression).  The fixture is a valid RS save with one Bulbasaur at
party slot 0 (per libpksave/tests/fixtures/generate_fixtures.py).

Asserts:
  - stdout contains "party: OK" — party_scene's ctor BN_LOG marker,
    proving the scene constructed.  Without this assertion the test
    can't distinguish "we entered party_scene and came back" from
    "main_menu's A on Party was a no-op the whole time".
  - the dumped FLASH still decodes as a valid RS save and the trainer
    name is still "RED" (the fixture's canonical name).  Save & Exit
    flushes the EWRAM mirror to FLASH and rewrites both banks; a
    spurious save during nav would either corrupt the slot-0 checksum
    or wipe the trainer block.  test_trainer_name_roundtrip.py is the
    positive control proving this assertion does catch real edits.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_party_scene_nav.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_party_nav.lua"
FIXTURE_NAME = "rs.sav"
EXPECTED_GAME = pk.Game.RS
EXPECTED_TRAINER_NAME = "RED"
FRAMES = "280"
TIMEOUT_SECONDS = 120


def fail(msg, result=None):
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout (tail) ---\n{result.stdout[-2000:]}", file=sys.stderr)
        print(f"--- stderr (tail) ---\n{result.stderr[-1000:]}", file=sys.stderr)
    sys.exit(1)


def main():
    lib = pk.load_library()
    fixture_src = os.path.join(REPO_ROOT, "libpksave", "tests", "fixtures", FIXTURE_NAME)
    if not os.path.isfile(fixture_src):
        fail(f"fixture missing: {fixture_src}")

    with tempfile.TemporaryDirectory(prefix="pksave-party-nav-") as workdir:
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
        if "drive: OK" not in result.stdout:
            fail("'drive: OK' marker not found", result)
        if "party: OK" not in result.stdout:
            fail("'party: OK' marker not found — party_scene did not "
                 "construct (main menu wiring or scene ctor regressed)", result)

        dump_path = os.path.join(dump_dir, "sram.bin")
        if not os.path.isfile(dump_path):
            fail(f"dump not produced: {dump_path}", result)

        with open(dump_path, "rb") as f:
            sram = f.read()
        if len(sram) != 131072:
            fail(f"expected 128 KiB FLASH dump, got {len(sram)}", result)

        detected = pk.detect(sram, lib=lib)
        if detected != EXPECTED_GAME:
            fail(f"detect {detected.name} != {EXPECTED_GAME.name} — slot 0 "
                 f"corrupted, possibly a stray save flush during nav", result)

        save = pk.open(sram, detected, lib=lib)
        try:
            name = pk.get_trainer_name(save)
        finally:
            pk.close(save)

        if name != EXPECTED_TRAINER_NAME:
            fail(f"trainer name {name!r} != {EXPECTED_TRAINER_NAME!r} — "
                 f"save_buffer was mutated and flushed; expected pure nav", result)

        print(f"PASS: party_scene nav (enter/scroll/exit); "
              f"detect={detected.name} trainer={name!r}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
