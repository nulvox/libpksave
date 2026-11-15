#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""
Framebuffer regression test for save_scene's blocking splash and input gate.

Closes the final acceptance criterion in docs/frontend-gba-plan.md:
"save scene blocks input while the FLASH write is in flight".

What's verified:
  1. PRE_SAVE  — main_menu at boot with cursor on row 0 ("> Trainer").
  2. SAVING    — mid-save snapshot taken after pressing A on Save & Exit while
                 DOWN taps are injected:
                 - Header strip (y=0..32) has 0 non-black pixels (main_menu's
                   header is gone).
                 - Center splash band (y=56..104, where "SAVING..." and
                   "DON'T TURN OFF THE POWER!" render at y=-16 and y=8) has
                   rendered glyph ink (>100 non-black pixels), and its full
                   framebuffer hash differs from PRE_SAVE.
  3. POST_SAVE — after save_scene completes flush_to_flash() and returns to
                 main_menu, the full 240x160 framebuffer hash equals PRE_SAVE
                 byte-for-byte, proving that the D-pad DOWN inputs injected
                 during the save splash were ignored and the cursor remains
                 on "> Trainer" (row 0).

Run from the repo root:
    python3 frontend-gba/tests/automation/test_save_blocks_input.py
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
import framebuffer as fbm  # noqa: E402

DOCKER_IMAGE = "pksave-test-gba"
EDITOR_ROM_CONTAINER = "/src/frontend-gba/pksave-gba.gba"
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_save_blocks_input.lua"
FRAMES = "360"
TIMEOUT_SECONDS = 240

FIXTURE_NAME = "rs.sav"


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

    with tempfile.TemporaryDirectory(prefix="pksave-save-blocks-") as workdir:
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
        if "save_blocks_input: OK" not in result.stdout:
            fail("'save_blocks_input: OK' marker not found", result)
        if "save: flush start" not in result.stdout or "save: flush done ok=true" not in result.stdout:
            fail("save_scene flush markers not logged", result)

        snaps = {
            "PRE":    os.path.join(dump_dir, "fb_pre.ppm"),
            "SAVING": os.path.join(dump_dir, "fb_saving.ppm"),
            "POST":   os.path.join(dump_dir, "fb_post.ppm"),
        }
        for label, path in snaps.items():
            if not os.path.isfile(path):
                fail(f"snapshot {label} missing at {path}", result)

        fb_pre    = fbm.Framebuffer.load(snaps["PRE"])
        fb_saving = fbm.Framebuffer.load(snaps["SAVING"])
        fb_post   = fbm.Framebuffer.load(snaps["POST"])

        pre_full_hash    = fb_pre.region_hash(0, 0, fbm.GBA_WIDTH, fbm.GBA_HEIGHT)
        saving_full_hash = fb_saving.region_hash(0, 0, fbm.GBA_WIDTH, fbm.GBA_HEIGHT)
        post_full_hash   = fb_post.region_hash(0, 0, fbm.GBA_WIDTH, fbm.GBA_HEIGHT)

        saving_header_ink = fbm.header_strip_non_black(fb_saving)
        saving_center_ink = fb_saving.count_non_black(0, 56, fbm.GBA_WIDTH, 48)

        print(f"  PRE    full_hash={pre_full_hash}")
        print(f"  SAVING full_hash={saving_full_hash}  header_ink={saving_header_ink}  center_ink={saving_center_ink}")
        print(f"  POST   full_hash={post_full_hash}")

        if saving_header_ink != 0:
            fail(f"SAVING header strip has {saving_header_ink} non-black pixels "
                 f"(expected 0 — main_menu header should not be visible during "
                 f"save_scene splash)", result)

        if saving_center_ink < 100:
            fail(f"SAVING center splash band has only {saving_center_ink} "
                 f"non-black pixels (<100); 'SAVING...' splash did not render",
                 result)

        if saving_full_hash == pre_full_hash:
            fail("SAVING framebuffer hash equals PRE framebuffer hash — "
                 "save_scene splash was not displayed", result)

        if post_full_hash != pre_full_hash:
            fail(f"POST full hash ({post_full_hash}) != PRE full hash "
                 f"({pre_full_hash}) — D-pad input during save_scene moved "
                 f"the cursor or save_scene did not return cleanly to "
                 f"main_menu row 0", result)

    print(f"PASS: save_scene displays splash (center_ink={saving_center_ink}) "
          f"and blocks input (PRE == POST full hash {pre_full_hash})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
