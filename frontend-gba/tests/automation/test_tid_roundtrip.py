#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
TID round-trip through the editor UI — exercises the numeric_field widget.

Mirrors the trainer-name Gate-J test but targets a different field and a
different widget shape: numeric_field uses keypad::*_held() with a hold-
counter (see frontend-gba/src/widgets/numeric_field.cpp). A 3-frame tap
on a direction produces exactly one step because the dead zone begins
at hold frame 2 and ends at INITIAL_HOLD_DELAY=10.

Quirk discovered while writing this test: list_view (the trainer
scene's row selector) consumes UP/DOWN edges *before* the per-row
widgets tick. So pressing UP/DOWN to bump TID actually navigates to
the previous/next row. The L/R axis is the only one list_view
ignores, so RIGHT (+10) / LEFT (-10) are the deterministic editing
keys.

(Originally the DOWN-to-arrive-on-ROW_TID transition also debited TID
by 1 because numeric_field treated the still-held DOWN edge as a fresh
frame-1 step; that bug was fixed in numeric_field via on_focus_acquired
+ per-axis arming. trainer_scene now disarms the destination field on
list-view selection change, so navigating onto a numeric row is a
no-op for the field. See numeric_field.cpp + trainer_scene.cpp.)

Sequence: boot → main_menu → trainer → DOWN (ROW_NAME→ROW_TID; no TID
change) → RIGHT×3 (tid +=30) → B back to main_menu → DOWN×4 → A
(Save & Exit) → FLASH write → dump → ctypes decode.

INITIAL_TID is hardcoded to the value generate_fixtures.py writes to
rs.sav (TEST_TID=0x1234). If the fixture generator changes, this test
fails informatively and points at the constant to update.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_tid_roundtrip.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_tid_edit_save.lua"
FRAMES = "480"
TIMEOUT_SECONDS = 180

FIXTURE_NAME = "rs.sav"
EXPECTED_GAME = pk.Game.RS

# From libpksave/tests/fixtures/generate_fixtures.py: TEST_TID = 0x1234.
# The rs.sav fixture writes this into Section 0 of the Gen3 save block.
INITIAL_TID = 0x1234

# Three 3-frame RIGHT taps (+10 each = +30). Net delta is now exactly
# +30 after the on_focus_acquired fix landed in numeric_field — the
# previous -1 side effect on DOWN navigation is gone.
TID_DELTA = 30
EXPECTED_TID = INITIAL_TID + TID_DELTA

# Name must be unchanged from the fixture: we never touch ROW_NAME.
ORIGINAL_TRAINER_NAME = "RED"


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

    with tempfile.TemporaryDirectory(prefix="pksave-tid-") as workdir:
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
        if "tid: OK" not in result.stdout:
            fail("'tid: OK' marker not found", result)

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
            trainer = pk.get_trainer(save)
            name = trainer.name.decode("ascii", errors="replace").rstrip("\x00")
            tid = trainer.tid
        finally:
            pk.close(save)

        print(f"  trainer name:  {name!r} (must equal {ORIGINAL_TRAINER_NAME!r})")
        print(f"  trainer tid:   {tid} (was {INITIAL_TID}, expected {EXPECTED_TID})")

        if name != ORIGINAL_TRAINER_NAME:
            fail(f"trainer name {name!r} != {ORIGINAL_TRAINER_NAME!r}; "
                 f"the TID drive script unexpectedly modified the name field "
                 f"(or the fixture generator changed)", result)

        if tid == INITIAL_TID:
            fail(f"TID unchanged ({tid}); numeric_field edit did not commit "
                 f"or save did not flush to FLASH. Check drive script timing "
                 f"and that the cursor landed on ROW_TID before UP taps.",
                 result)

        if tid != EXPECTED_TID:
            fail(f"TID {tid} != expected {EXPECTED_TID}; delta was "
                 f"{tid - INITIAL_TID}, expected {TID_DELTA}. "
                 f"numeric_field's hold/repeat behaviour may have shifted — "
                 f"re-read numeric_field.cpp `should_step_this_frame` and "
                 f"adjust h.tap hold or inter-tap gaps in the drive script.",
                 result)

    print(f"PASS: TID round-trip — UI drove {INITIAL_TID} → {tid} "
          f"(+{tid - INITIAL_TID}) through FLASH; name preserved as "
          f"{name!r}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
