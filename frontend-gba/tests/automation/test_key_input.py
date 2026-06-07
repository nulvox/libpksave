#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Test: a single A-button tap driven from Lua is observable inside the GBA
core via the KEYINPUT register (0x04000130).

The companion Lua script `tap_a_probe.lua` drives the input and emits
per-frame KEYINPUT samples on stdout. We parse those samples here and
re-assert the same invariants on the Python side so a failure produces a
clean message in the test runner output (in addition to the Lua-side
assertions that also non-zero-exit the container on failure).

Idle GBA KEYINPUT (no keys held) reads 0x03ff because the register is
active-low and bits 0..9 correspond to A, B, Select, Start, Right, Left,
Up, Down, R, L. A single A tap clears bit 0 -> 0x03fe.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_key_input.py
"""

import os
import re
import subprocess
import sys

DOCKER_IMAGE = "pksave-test-gba"
SMOKE_ROM_CONTAINER = "/opt/mgba-test/smoke.gba"

_HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", ".."))
TAP_LUA_CONTAINER = "/src/frontend-gba/tests/automation/tap_a_probe.lua"

# The Lua probe presses A on frame 3 and releases on frame 4; run enough
# frames for the press+release cycle plus a couple of trailing idle samples.
FRAMES = "6"
TIMEOUT_SECONDS = 60

# Match lines like "Scripting: tapA: frame=3 KEYINPUT=0x03fe bitA=0".
_FRAME_RE = re.compile(
    r"tapA: frame=(?P<frame>\d+)\s+KEYINPUT=0x(?P<ki>[0-9a-fA-F]+)\s+bitA=(?P<bit>\d)"
)


def fail(msg, result=None):
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout ---\n{result.stdout}", file=sys.stderr)
        print(f"--- stderr ---\n{result.stderr}", file=sys.stderr)
    sys.exit(1)


def parse_samples(stdout):
    samples = {}
    for line in stdout.splitlines():
        m = _FRAME_RE.search(line)
        if m:
            samples[int(m["frame"])] = int(m["ki"], 16)
    return samples


def main():
    try:
        result = subprocess.run(
            ["docker", "run", "--rm",
             "-v", f"{REPO_ROOT}:/src:ro",
             DOCKER_IMAGE,
             "mgba-lua", SMOKE_ROM_CONTAINER, TAP_LUA_CONTAINER, FRAMES],
            capture_output=True, text=True, timeout=TIMEOUT_SECONDS,
        )
    except FileNotFoundError:
        fail("docker not found on PATH")
    except subprocess.TimeoutExpired:
        fail(f"mgba-lua did not exit within {TIMEOUT_SECONDS}s")

    if result.returncode != 0:
        fail(f"mgba-lua exited {result.returncode}", result)

    if "tapA: OK" not in result.stdout:
        fail("'tapA: OK' marker not found", result)

    samples = parse_samples(result.stdout)
    if not samples:
        fail("no KEYINPUT samples parsed from stdout", result)

    # Idle frames (1, 2): bit 0 set (A unpressed), all 10 key bits set.
    for f in (1, 2):
        v = samples.get(f)
        if v is None:
            fail(f"missing idle sample for frame {f}", result)
        if (v & 0x3ff) != 0x3ff:
            fail(f"frame {f} idle KEYINPUT=0x{v:04x}, expected low10=0x3ff", result)

    # Press frame (3): bit 0 cleared (A held), all other key bits still set.
    vp = samples.get(3)
    if vp is None:
        fail("missing press sample for frame 3", result)
    if (vp & 0x001) != 0:
        fail(f"press frame KEYINPUT=0x{vp:04x}, bit0 (A) should be 0", result)
    if (vp & 0x3fe) != 0x3fe:
        fail(f"press frame KEYINPUT=0x{vp:04x}, other keys should still be 1", result)

    # Release frame (4): bit 0 set again.
    vr = samples.get(4)
    if vr is None:
        fail("missing release sample for frame 4", result)
    if (vr & 0x3ff) != 0x3ff:
        fail(f"release frame KEYINPUT=0x{vr:04x}, expected 0x3ff", result)

    print(
        "PASS: A-button tap observable via emu:setKeys -> KEYINPUT; "
        f"idle=0x{samples[1]:04x} press=0x{vp:04x} release=0x{vr:04x}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
