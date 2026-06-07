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

# Per-variant matrix. All three Gen-3 fixtures share the canonical initial
# trainer name "RED" (per generate_fixtures.py). Three A taps inside
# text_input append "AAA" → "REDAAA" regardless of variant.
ORIGINAL_TRAINER_NAME = "RED"
EXPECTED_TRAINER_NAME = "REDAAA"
CASES = [
    ("rs.sav",      pk.Game.RS),
    ("emerald.sav", pk.Game.EMERALD),
    ("frlg.sav",    pk.Game.FRLG),
]


def fail(msg, result=None):
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout (tail) ---\n{result.stdout[-2000:]}", file=sys.stderr)
        print(f"--- stderr (tail) ---\n{result.stderr[-1000:]}", file=sys.stderr)
    sys.exit(1)


def run_case(fixture_name, expected_game, lib):
    fixture_src = os.path.join(REPO_ROOT, "libpksave", "tests", "fixtures", fixture_name)
    if not os.path.isfile(fixture_src):
        fail(f"fixture missing: {fixture_src}")

    with tempfile.TemporaryDirectory(prefix=f"pksave-j-{fixture_name}-") as workdir:
        dump_dir = os.path.join(workdir, "dump")
        save_dir = os.path.join(workdir, "save")
        os.makedirs(dump_dir)
        os.makedirs(save_dir)

        save_path = os.path.join(save_dir, fixture_name)
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
                 "--save", f"/save/{fixture_name}"],
                capture_output=True, text=True, timeout=TIMEOUT_SECONDS,
            )
        except FileNotFoundError:
            fail("docker not found on PATH")
        except subprocess.TimeoutExpired:
            fail(f"{fixture_name}: mgba-lua did not exit within {TIMEOUT_SECONDS}s")

        if result.returncode != 0:
            fail(f"{fixture_name}: mgba-lua exited {result.returncode}", result)
        if "j: OK" not in result.stdout:
            fail(f"{fixture_name}: 'j: OK' marker not found", result)

        dump_path = os.path.join(dump_dir, "sram.bin")
        if not os.path.isfile(dump_path):
            fail(f"{fixture_name}: dump not produced", result)

        with open(dump_path, "rb") as f:
            sram = f.read()
        if len(sram) != 131072:
            fail(f"{fixture_name}: expected 128 KiB FLASH, got {len(sram)}", result)

        detected = pk.detect(sram, lib=lib)
        if detected != expected_game:
            fail(f"{fixture_name}: expected {expected_game.name}, got {detected.name}", result)

        save = pk.open(sram, detected, lib=lib)
        try:
            name = pk.get_trainer_name(save)
        finally:
            pk.close(save)

        if name == ORIGINAL_TRAINER_NAME:
            fail(f"{fixture_name}: trainer name unchanged ({name!r}); edit did not "
                 f"commit or save did not flush to FLASH", result)
        if name != EXPECTED_TRAINER_NAME:
            fail(f"{fixture_name}: trainer name {name!r} != expected "
                 f"{EXPECTED_TRAINER_NAME!r}; variant-specific encoding bug?", result)

        print(f"  OK  {fixture_name:12s} detect={detected.name:7s} {ORIGINAL_TRAINER_NAME!r} → {name!r}")


def main():
    lib = pk.load_library()
    print(f"running {len(CASES)} Gen-3 trainer-name round-trip cases:")
    for fixture_name, expected_game in CASES:
        run_case(fixture_name, expected_game, lib)
    print(f"PASS: {len(CASES)}/{len(CASES)} variants edited 'RED' → 'REDAAA' through FLASH")
    return 0


if __name__ == "__main__":
    sys.exit(main())
