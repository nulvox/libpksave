#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Gate J — full trainer-name round-trip through the editor UI.

Original tracer-bullet target from docs/frontend-gba-plan.md §"mGBA Lua
automation". Combines every harness primitive shipped by the POC and the
five parallel tracks:

    Track C: sidecar `--save` load of rs.sav fixture
    Track D: emu:setKeys drives the editor's keypad
    Track A: FLASH 128 KiB detection (test_flash_detected confirms)
    Track E: full SRAM/FLASH dump via mirror probe
    Track F: helpers.lua centralises tap/at/sram_dump

Sequence: boot → main_menu → trainer → text_input → type "AAA" → commit
→ back to main_menu → Save & Exit → FLASH write → dump → ctypes decode.

Asserts the dumped FLASH carries trainer.name == "REDAAA" (the original
"RED" extended with three A's from the text_input keyboard).

Run from the repo root:
    python3 frontend-gba/tests/automation/test_trainer_name_roundtrip.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_name_edit_save.lua"
FRAMES = "480"
TIMEOUT_SECONDS = 180

FIXTURE_NAME = "rs.sav"
EXPECTED_GAME = pk.Game.RS
ORIGINAL_TRAINER_NAME = "RED"
EXPECTED_TRAINER_NAME = "REDAAA"


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

    with tempfile.TemporaryDirectory(prefix="pksave-j-") as workdir:
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
        if "j: OK" not in result.stdout:
            fail("'j: OK' marker not found", result)

        dump_path = os.path.join(dump_dir, "sram.bin")
        if not os.path.isfile(dump_path):
            fail(f"dump not produced: {dump_path}", result)

        with open(dump_path, "rb") as f:
            sram = f.read()

        print(f"  dump size:     {len(sram)} bytes")
        if len(sram) != 131072:
            fail(f"expected 128 KiB FLASH, got {len(sram)}", result)

        lib = pk.load_library()
        detected = pk.detect(sram, lib=lib)
        print(f"  pksave_detect: {detected.name}")
        if detected != EXPECTED_GAME:
            fail(f"expected {EXPECTED_GAME.name}, got {detected.name}", result)

        save = pk.open(sram, detected, lib=lib)
        try:
            name = pk.get_trainer_name(save)
        finally:
            pk.close(save)

        print(f"  trainer name:  {name!r} (was {ORIGINAL_TRAINER_NAME!r})")
        if name == ORIGINAL_TRAINER_NAME:
            fail(f"trainer name unchanged ({name!r}); edit did not commit "
                 f"or save did not flush to FLASH", result)
        if name != EXPECTED_TRAINER_NAME:
            fail(f"trainer name {name!r} != expected {EXPECTED_TRAINER_NAME!r}; "
                 f"keystroke sequence may not match the editor's actual scene "
                 f"transitions", result)

    print(f"PASS: trainer-name round-trip — UI drive '{ORIGINAL_TRAINER_NAME}' → '{name}' through FLASH")
    return 0


if __name__ == "__main__":
    sys.exit(main())
