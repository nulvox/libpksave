#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Tracer-bullet test for the mGBA-Lua automation pipeline.

Proves that Python can drive the docker/test-gba container end-to-end:
spawn `mgba-lua` against the bundled smoke ROM + smoke script, and observe
the script's stdout reach this process.

This is the smallest meaningful step toward the trainer-name SRAM
round-trip; it asserts only that the harness is reachable from Python.
Behavior tests (variant detect, scene drive, save flow) follow.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_mgba_harness_smoke.py
"""

import subprocess
import sys

DOCKER_IMAGE = "pksave-test-gba"
SMOKE_ROM = "/opt/mgba-test/smoke.gba"
SMOKE_LUA = "/opt/mgba-test/smoke.lua"
SMOKE_FRAMES = "60"
EXPECTED_MARKER = "smoke: OK"
TIMEOUT_SECONDS = 60


def run_harness():
    return subprocess.run(
        ["docker", "run", "--rm", DOCKER_IMAGE,
         "mgba-lua", SMOKE_ROM, SMOKE_LUA, SMOKE_FRAMES],
        capture_output=True, text=True, timeout=TIMEOUT_SECONDS,
    )


def fail(msg, result=None):
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout ---\n{result.stdout}", file=sys.stderr)
        print(f"--- stderr ---\n{result.stderr}", file=sys.stderr)
    sys.exit(1)


def main():
    try:
        result = run_harness()
    except FileNotFoundError:
        fail("docker not found on PATH")
    except subprocess.TimeoutExpired:
        fail(f"mgba-lua did not exit within {TIMEOUT_SECONDS}s")

    if result.returncode != 0:
        fail(f"mgba-lua exited {result.returncode}", result)
    if EXPECTED_MARKER not in result.stdout:
        fail(f"expected marker {EXPECTED_MARKER!r} not in stdout", result)

    print(f"PASS: mgba-lua harness reachable from Python; marker {EXPECTED_MARKER!r} observed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
