#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Test 2: prove the real frontend-gba editor ROM loads under mgba-lua via a
host-mounted volume.

Different from test_mgba_harness_smoke.py in two ways:
  1. ROM comes from the host repo (mounted at /src), not from the image.
  2. Assertion parses the script's `platform=N` line and confirms N==0
     (mGBA's GBA platform constant), proving the cartridge actually loaded
     as a GBA core rather than just that the harness exited cleanly.

The Lua script is the bundled /opt/mgba-test/smoke.lua — no new scripting
needed for this slice.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_editor_rom_loads.py
"""

import os
import re
import subprocess
import sys

DOCKER_IMAGE = "pksave-test-gba"

_HERE = os.path.dirname(os.path.abspath(__file__))
# frontend-gba/tests/automation/ → ../../..
REPO_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", ".."))
EDITOR_ROM_HOST = os.path.join(REPO_ROOT, "frontend-gba", "pksave-gba.gba")
EDITOR_ROM_CONTAINER = "/src/frontend-gba/pksave-gba.gba"
SMOKE_LUA_CONTAINER = "/opt/mgba-test/smoke.lua"
FRAMES = "60"
TIMEOUT_SECONDS = 60

MGBA_PLATFORM_GBA = 0


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

    # Parse the platform from smoke.lua's stdout line.
    # Format: "Scripting: smoke: platform=N frame=M"
    m = re.search(r"smoke: platform=(\d+)\s+frame=", result.stdout)
    if not m:
        fail("no `platform=N` line found in stdout", result)
    platform = int(m.group(1))
    if platform != MGBA_PLATFORM_GBA:
        fail(f"expected platform={MGBA_PLATFORM_GBA} (GBA), got {platform}", result)

    if "smoke: OK" not in result.stdout:
        fail("'smoke: OK' marker not found", result)

    print(f"PASS: editor ROM {os.path.basename(EDITOR_ROM_HOST)} loaded as platform={platform}; harness ran {FRAMES} frames cleanly")
    return 0


if __name__ == "__main__":
    sys.exit(main())
