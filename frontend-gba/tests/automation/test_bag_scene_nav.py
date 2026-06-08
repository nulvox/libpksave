#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
CP#4 nav test — drive bag_scene navigation and assert no FLASH writes.

Loads rs.sav as a sidecar (without it, pksave_detect returns UNKNOWN and
the editor halts on boot_error_scene, so the drive never reaches the
bag).  Drives: main_menu → DOWN×2 → A → bag_scene → RIGHT×2 → LEFT → B
→ main_menu.

Asserts:
  - stdout contains "bag: OK" — bag_scene's ctor BN_LOG marker, proving
    the scene constructed.
  - the dumped FLASH still decodes as a valid RS save and the trainer
    name is still "RED".  A spurious save during nav would corrupt the
    slot-0 checksum.  test_bag_slot_edit_roundtrip.py is the positive
    control proving this assertion does catch real edits.
  - the Items pocket still contains the fixture's seeded slot 0 (Potion
    qty 5), confirming pocket-cycling didn't write the live save.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_bag_scene_nav.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_bag_nav.lua"
FIXTURE_NAME = "rs.sav"
EXPECTED_GAME = pk.Game.RS
EXPECTED_TRAINER_NAME = "RED"
# Fixture seeds RS items pocket with Potion (id=13) ×5, item-0x44 ×2.
EXPECTED_SLOT_0_ITEM_ID = 13
EXPECTED_SLOT_0_QTY     = 5
FRAMES = "320"
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

    with tempfile.TemporaryDirectory(prefix="pksave-bag-nav-") as workdir:
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
        if "bag: OK" not in result.stdout:
            fail("'bag: OK' marker not found — bag_scene did not "
                 "construct (main_menu A on Bag, scene_kind dispatch, or "
                 "bag_scene's ctor regressed)", result)
        if "bag_nav: OK" not in result.stdout:
            fail("'bag_nav: OK' marker not found (drive script "
                 "did not reach the shutdown callback)", result)

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
            items, items_count = pk.get_pocket(save, pk.Pocket.ITEMS)
        finally:
            pk.close(save)

        if name != EXPECTED_TRAINER_NAME:
            fail(f"trainer name {name!r} != {EXPECTED_TRAINER_NAME!r} — "
                 f"save_buffer was mutated and flushed; expected pure nav", result)

        if items_count < 1 or items[0].item_id != EXPECTED_SLOT_0_ITEM_ID \
                or items[0].quantity != EXPECTED_SLOT_0_QTY:
            fail(f"items pocket slot 0 = ({items[0].item_id}, {items[0].quantity}) "
                 f"!= expected ({EXPECTED_SLOT_0_ITEM_ID}, {EXPECTED_SLOT_0_QTY}); "
                 f"nav unexpectedly mutated the bag", result)

        print(f"PASS: bag_scene nav (open/cycle pockets/return); "
              f"detect={detected.name} trainer={name!r} "
              f"items[0]=({items[0].item_id}, {items[0].quantity})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
