#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Framebuffer-byte regression test for the dirty-indicator round-trip.

Closes the dirty-indicator gap filed in
docs/frontend-gba-plan.md §"Acceptance criteria" ("dirty indicator
appears after an edit and clears after save — needs framebuffer
assertion infra"). That gap had been open since CP#1 because the
mGBA-Lua harness had no way to assert on rendered pixels — only on
SRAM/FLASH bytes.

What's verified:

  CLEAN      — main_menu header is "POKEMON RUBY" (no asterisk).
               Snapshot of the top 32 rows hashed as the baseline.

  DIRTY      — after the drive script bumps TID via numeric_field and
               returns to main_menu, the bridge is marked dirty (see
               trainer_scene.cpp:75 push_to_save → mark_dirty) and
               main_menu_scene::redraw() appends " *" to the centered
               header (main_menu_scene.cpp:53-56). Centre alignment
               shifts the existing glyphs left to make room, so the
               header_strip hash MUST differ from CLEAN. The asterisk
               glyph also adds non-black ink, so the non-black pixel
               count MUST be strictly greater than CLEAN.

  AFTER_SAVE — after Save & Exit clears the dirty flag via flush_to_flash
               (pksave_bridge.cpp:88) and the redraw cycle observes the
               flip back to false, the header is "POKEMON RUBY" again.
               Its hash MUST equal CLEAN's hash, byte for byte.

The strip hash assertion is the load-bearing one — it requires the
header to render IDENTICALLY before-and-after a dirty/save cycle. A
butano version bump that changes glyph metrics, a font swap, or a
header text reformat would all surface here as a CLEAN vs AFTER_SAVE
mismatch. That's the test working — re-bless by deleting and re-running
once the new rendering is intentional.

If this test ever flakes for timing reasons (e.g. a save_scene splash
takes longer than the 80-frame slack), bump FRAMES + the at()-snapshot
frame numbers in drive_dirty_indicator.lua together; the at() callback
fires AT the specified frame, not "at frame N if rendered by then".

Run from the repo root:
    python3 frontend-gba/tests/automation/test_dirty_indicator.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_dirty_indicator.lua"
FRAMES = "540"  # last fb_dump at frame 480; small tail for shutdown
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

    with tempfile.TemporaryDirectory(prefix="pksave-dirty-") as workdir:
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
        if "dirty: OK" not in result.stdout:
            fail("'dirty: OK' marker not found", result)

        # All three snapshots must exist — a missing one means the at()
        # callback never fired (timing skew) or fb_dump_ppm errored.
        snaps = {
            "CLEAN":      os.path.join(dump_dir, "fb_clean.ppm"),
            "DIRTY":      os.path.join(dump_dir, "fb_dirty.ppm"),
            "AFTER_SAVE": os.path.join(dump_dir, "fb_after_save.ppm"),
        }
        for label, path in snaps.items():
            if not os.path.isfile(path):
                fail(f"snapshot {label} missing at {path} — drive_dirty_indicator.lua's "
                     f"h.fb_dump() never fired or fb_dump_ppm returned an error; "
                     f"check stdout for 'helpers.fb_dump' log lines",
                     result)

        fb_clean      = fbm.Framebuffer.load(snaps["CLEAN"])
        fb_dirty      = fbm.Framebuffer.load(snaps["DIRTY"])
        fb_after_save = fbm.Framebuffer.load(snaps["AFTER_SAVE"])

        for label, fb in [("CLEAN", fb_clean), ("DIRTY", fb_dirty), ("AFTER_SAVE", fb_after_save)]:
            if (fb.width, fb.height) != (fbm.GBA_WIDTH, fbm.GBA_HEIGHT):
                fail(f"snapshot {label} dims {fb.width}x{fb.height} != "
                     f"{fbm.GBA_WIDTH}x{fbm.GBA_HEIGHT}", result)

        clean_hash      = fbm.header_strip_hash(fb_clean)
        dirty_hash      = fbm.header_strip_hash(fb_dirty)
        after_save_hash = fbm.header_strip_hash(fb_after_save)

        clean_ink      = fbm.header_strip_non_black(fb_clean)
        dirty_ink      = fbm.header_strip_non_black(fb_dirty)
        after_save_ink = fbm.header_strip_non_black(fb_after_save)

        print(f"  header strip y=[{fbm.HEADER_STRIP_Y0}..{fbm.HEADER_STRIP_Y1})  width={fbm.GBA_WIDTH}")
        print(f"  CLEAN      hash={clean_hash}  non-black={clean_ink}")
        print(f"  DIRTY      hash={dirty_hash}  non-black={dirty_ink}")
        print(f"  AFTER_SAVE hash={after_save_hash}  non-black={after_save_ink}")

        # Sanity: there's actually rendered text in the header strip.
        # If the boot_error scene fired (e.g., fixture didn't load), the
        # header strip would be all-black and clean_ink would be ~0.
        if clean_ink < 50:
            fail(f"CLEAN header strip has only {clean_ink} non-black pixels "
                 f"(<50); the editor may have landed on boot_error_scene "
                 f"instead of main_menu_scene. Check sidecar save load.",
                 result)

        # Hash assertions.
        if dirty_hash == clean_hash:
            fail(f"DIRTY hash ({dirty_hash}) == CLEAN hash ({clean_hash}) — "
                 f"the trainer-scene edit DID NOT flip the header to include "
                 f"the ' *' dirty indicator. Likely causes: TID edit didn't "
                 f"commit (check drive_tid_edit_save.lua timing), or "
                 f"main_menu_scene::update() no longer redraws on dirty-flag "
                 f"transition (check main_menu_scene.cpp:112).",
                 result)

        if after_save_hash != clean_hash:
            fail(f"AFTER_SAVE hash ({after_save_hash}) != CLEAN hash "
                 f"({clean_hash}) — the dirty indicator did NOT clear after "
                 f"Save & Exit. Likely causes: flush_to_flash() failed "
                 f"(check pksave_bridge.cpp), save_scene didn't return to "
                 f"main_menu after the splash, or main_menu didn't re-render "
                 f"with the new dirty=false state. If the diff is small and "
                 f"intentional (font/glyph change), re-bless by re-running.",
                 result)

        # Directional assertion: the asterisk adds glyph ink even after
        # centre-realignment, since '*' is wider than the implicit trailing
        # whitespace in the original string.
        if dirty_ink <= clean_ink:
            fail(f"DIRTY non-black pixel count ({dirty_ink}) is NOT strictly "
                 f"greater than CLEAN ({clean_ink}); expected the ' *' suffix "
                 f"to add glyph ink. Either the asterisk is being drawn in "
                 f"the background colour (very unlikely) or the dirty header "
                 f"is being rendered at a different y-band — widen "
                 f"HEADER_STRIP_Y1 in framebuffer.py to cover.",
                 result)

    print(f"PASS: dirty indicator — CLEAN→DIRTY→AFTER_SAVE cycles the "
          f"header strip hash {clean_hash} → {dirty_hash} → {clean_hash} "
          f"with the '*' adding {dirty_ink - clean_ink} non-black pixels")
    return 0


if __name__ == "__main__":
    sys.exit(main())
