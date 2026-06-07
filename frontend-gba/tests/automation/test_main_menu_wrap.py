#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
main_menu cursor wrap test.

Proves the cursor in main_menu_scene wraps from index 4 (Save & Exit) back
to index 0 (Trainer) when DOWN is held past the end.

main_menu_scene.cpp uses `cursor_ = (cursor_ + 1) % ENTRY_COUNT` on every
DOWN press, so 5 DOWNs from the initial cursor=0 should walk
    0 → 1 → 2 → 3 → 4 → 0
landing back on Trainer.  We verify this by:

    1. Driving 5 DOWN taps (would-be wrap).
    2. A → if wrap worked, enters trainer_scene; otherwise enters save_scene
       (cursor would be stuck at 4).
    3. A → text_input on the trainer name field.
    4. A → place one 'A' char; "RED" → "REDA".
    5. START → commit text_input.
    6. B → return to main_menu.
    7. DOWN ×4 + A → save & exit, flush FLASH.
    8. Read back trainer.name from the dumped FLASH.

Pass: trainer.name == "REDA" (wrap worked + Trainer reachable + edit saved).
Fail: trainer.name == "RED"  → wrap broken (or timing skipped text_input).
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_wrap_then_edit.lua"
FRAMES = "600"
TIMEOUT_SECONDS = 240

FIXTURE_NAME = "rs.sav"
EXPECTED_GAME = pk.Game.RS
ORIGINAL_TRAINER_NAME = "RED"
EXPECTED_TRAINER_NAME = "REDA"


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

    with tempfile.TemporaryDirectory(prefix="pksave-wrap-") as workdir:
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
        if "wrap: OK" not in result.stdout:
            fail("'wrap: OK' marker not found", result)

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
            fail(
                f"trainer name unchanged ({name!r}); wrap likely broken — "
                f"the 6th tap (A) hit Save & Exit instead of Trainer, so the "
                f"text_input edit never happened. main_menu_scene.cpp DOWN "
                f"handler should wrap (cursor + 1) % ENTRY_COUNT.",
                result,
            )
        if name != EXPECTED_TRAINER_NAME:
            fail(
                f"trainer name {name!r} != expected {EXPECTED_TRAINER_NAME!r}; "
                f"wrap may have worked but the text_input sequence diverged "
                f"(check inter-tap gaps in drive_wrap_then_edit.lua).",
                result,
            )

    print(f"PASS: main_menu wrap — 5×DOWN wrapped to Trainer; "
          f"'{ORIGINAL_TRAINER_NAME}' → '{name}' through FLASH")
    return 0


if __name__ == "__main__":
    sys.exit(main())
