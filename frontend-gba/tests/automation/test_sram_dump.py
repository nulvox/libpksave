#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Test 3: Lua can dump SRAM bytes to a host-mounted file, and Python can
read them back.

Proves the data path Lua → file → Python that the eventual SRAM-roundtrip
assertion needs. We only dump the first 16 bytes here; expanding to a full
128 KiB FLASH dump is the next slice.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_sram_dump.py
"""

import os
import subprocess
import sys
import tempfile

DOCKER_IMAGE = "pksave-test-gba"

_HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", ".."))
EDITOR_ROM_CONTAINER = "/src/frontend-gba/pksave-gba.gba"
DUMP_LUA_CONTAINER = "/src/frontend-gba/tests/automation/dump_sram_head.lua"
EXPECTED_DUMP_LEN = 16
TIMEOUT_SECONDS = 60


def fail(msg, result=None):
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout ---\n{result.stdout}", file=sys.stderr)
        print(f"--- stderr ---\n{result.stderr}", file=sys.stderr)
    sys.exit(1)


def main():
    with tempfile.TemporaryDirectory(prefix="pksave-sram-") as dump_dir:
        try:
            result = subprocess.run(
                ["docker", "run", "--rm",
                 "-v", f"{REPO_ROOT}:/src:ro",
                 "-v", f"{dump_dir}:/dump:rw",
                 DOCKER_IMAGE,
                 "mgba-lua", EDITOR_ROM_CONTAINER, DUMP_LUA_CONTAINER, "10"],
                capture_output=True, text=True, timeout=TIMEOUT_SECONDS,
            )
        except FileNotFoundError:
            fail("docker not found on PATH")
        except subprocess.TimeoutExpired:
            fail(f"mgba-lua did not exit within {TIMEOUT_SECONDS}s")

        if result.returncode != 0:
            fail(f"mgba-lua exited {result.returncode}", result)
        if "dump: OK" not in result.stdout:
            fail("'dump: OK' marker not found", result)

        dump_path = os.path.join(dump_dir, "sram.bin")
        if not os.path.isfile(dump_path):
            fail(f"dump file not produced: {dump_path}", result)

        with open(dump_path, "rb") as f:
            data = f.read()

        if len(data) != EXPECTED_DUMP_LEN:
            fail(f"dump length {len(data)} != expected {EXPECTED_DUMP_LEN}", result)

        # Fresh SRAM on the editor ROM (no save loaded) is 0xFF.
        # We don't strictly assert the value pattern — only that the bytes
        # made it out — but a tiny sanity log helps debugging.
        head = " ".join(f"{b:02x}" for b in data)
        print(f"PASS: dumped {len(data)} bytes from SRAM via Lua → file → Python: {head}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
