#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Test: prove mGBA detects the editor ROM as FLASH 128 KiB, not SRAM.

Background
----------
mGBA picks the save backing by trapping the first cart-memory access:
  * a write to 0x0E005555 with no prior SRAM access  -> Flash
  * any other read/write to 0x0E000000..0x0E00FFFF   -> SRAM
A separate "Updating flash chip from 512kb to 1Mb" log fires once the
cartridge code performs a bank switch (proving the 128 KiB / 1 Mbit size,
which is what Gen III Pokemon save layouts need).

The editor ROM embeds the "FLASH1M_V103" magic string
(frontend-gba/src/save_type_signature.cpp) and its boot path calls
pksave_gba::flash_read_all() which issues a bank-switch command before
any SRAM access — that is what flips mGBA into FLASH-128 KiB mode.

What this test asserts
----------------------
  1. mgba-lua's stdout/stderr contains "Detected Flash savegame".
  2. mgba-lua's stdout/stderr contains "Updating flash chip from
     512kb to 1Mb" (proving the 128 KiB upsize, not 64 KiB FLASH).
  3. mgba-lua's stdout/stderr does NOT contain "Detected SRAM savegame".

The bundled /opt/mgba-test/smoke.lua is used as the scripting payload —
it doesn't read SRAM, so it won't trigger a false SRAM autodetect.

The harness needs more than smoke.lua's default 5 frames to reach the
flash bank-switch path (butano core init + our boot copy land around
frame 15-20).  60 frames is the chosen budget.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_flash_detected.py
"""

import os
import subprocess
import sys

DOCKER_IMAGE = "pksave-test-gba"

_HERE = os.path.dirname(os.path.abspath(__file__))
# frontend-gba/tests/automation/ -> ../../..
REPO_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", ".."))
EDITOR_ROM_HOST = os.path.join(REPO_ROOT, "frontend-gba", "pksave-gba.gba")
EDITOR_ROM_CONTAINER = "/src/frontend-gba/pksave-gba.gba"
SMOKE_LUA_CONTAINER = "/opt/mgba-test/smoke.lua"
FRAMES = "60"
TIMEOUT_SECONDS = 60

# Expected log lines from src/gba/memory.c and src/gba/savedata.c in mGBA.
FLASH_DETECTED = "Detected Flash savegame"
FLASH_UPGRADED = "Updating flash chip from 512kb to 1Mb"
SRAM_DETECTED = "Detected SRAM savegame"


def fail(msg, result=None):
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout ---\n{result.stdout}", file=sys.stderr)
        print(f"--- stderr ---\n{result.stderr}", file=sys.stderr)
    sys.exit(1)


def main():
    if not os.path.isfile(EDITOR_ROM_HOST):
        fail(f"editor ROM not found: {EDITOR_ROM_HOST} (run `make gba`?)")

    try:
        result = subprocess.run(
            ["docker", "run", "--rm",
             "-v", f"{REPO_ROOT}:/src:ro",
             DOCKER_IMAGE,
             "mgba-lua", EDITOR_ROM_CONTAINER, SMOKE_LUA_CONTAINER, FRAMES],
            capture_output=True, text=True, timeout=TIMEOUT_SECONDS,
        )
    except FileNotFoundError:
        fail("docker not found on PATH")
    except subprocess.TimeoutExpired:
        fail(f"mgba-lua did not exit within {TIMEOUT_SECONDS}s")

    if result.returncode != 0:
        fail(f"mgba-lua exited {result.returncode}", result)

    # mGBA's mLOG sink writes to stderr by default; capture both streams.
    combined = result.stdout + result.stderr

    if SRAM_DETECTED in combined:
        fail(
            f"emulator detected SRAM, not FLASH: saw {SRAM_DETECTED!r} in output",
            result,
        )

    if FLASH_DETECTED not in combined:
        fail(
            f"emulator never logged {FLASH_DETECTED!r} within {FRAMES} frames "
            f"(ROM may be missing FLASH1M_V magic or boot path didn't reach "
            f"flash_read_all)",
            result,
        )

    if FLASH_UPGRADED not in combined:
        fail(
            f"emulator detected FLASH but never upgraded to 1 Mbit "
            f"({FLASH_UPGRADED!r} not seen) — Gen III saves need the full "
            f"128 KiB backing",
            result,
        )

    print(
        f"PASS: editor ROM {os.path.basename(EDITOR_ROM_HOST)} -> "
        f"{FLASH_DETECTED!r} + {FLASH_UPGRADED!r}; "
        f"no SRAM autodetect within {FRAMES} frames"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
