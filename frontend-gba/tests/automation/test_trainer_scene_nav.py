#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Gate I — drive input through the main menu, assert pure navigation does
NOT mutate SRAM/FLASH.

Negative control: with no fixture loaded, the FLASH region is the
erased-state 0xFF. After booting + tapping A to select the Trainer
entry from the main menu, the dumped FLASH must still be all 0xFF.
This proves the editor's navigation path doesn't write SRAM (per
docs/frontend-gba-plan.md §"Build & save plumbing" #3: FLASH is
rewritten only by the explicit Save & Exit action).

Run from the repo root:
    python3 frontend-gba/tests/automation/test_trainer_scene_nav.py
"""

import os
import subprocess
import sys
import tempfile

DOCKER_IMAGE = "pksave-test-gba"

_HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", ".."))
EDITOR_ROM_CONTAINER = "/src/frontend-gba/pksave-gba.gba"
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_main_menu_a.lua"
FRAMES = "120"  # Allow generous time for boot + tap + scene transition
TIMEOUT_SECONDS = 120


def fail(msg, result=None):
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout ---\n{result.stdout}", file=sys.stderr)
        print(f"--- stderr ---\n{result.stderr}", file=sys.stderr)
    sys.exit(1)


def main():
    with tempfile.TemporaryDirectory(prefix="pksave-nav-") as workdir:
        dump_dir = os.path.join(workdir, "dump")
        os.makedirs(dump_dir)

        try:
            result = subprocess.run(
                ["docker", "run", "--rm",
                 "-v", f"{REPO_ROOT}:/src:ro",
                 "-v", f"{dump_dir}:/dump:rw",
                 DOCKER_IMAGE,
                 "mgba-lua",
                 EDITOR_ROM_CONTAINER, DRIVE_LUA_CONTAINER, FRAMES],
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

        dump_path = os.path.join(dump_dir, "sram.bin")
        if not os.path.isfile(dump_path):
            fail(f"dump not produced: {dump_path}", result)

        with open(dump_path, "rb") as f:
            sram = f.read()

        # Negative control: navigation must not mutate FLASH.
        # Erased FLASH = 0xFF for every byte.
        non_ff = [(i, b) for i, b in enumerate(sram) if b != 0xFF]
        if non_ff:
            fail(
                f"navigation mutated FLASH: {len(non_ff)} non-0xFF bytes "
                f"in {len(sram)}-byte dump. First 5: "
                f"{[(hex(i), hex(b)) for i, b in non_ff[:5]]}",
                result,
            )

        print(f"PASS: main-menu A-tap navigation; {len(sram)} bytes dumped, all 0xFF (no FLASH writes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
