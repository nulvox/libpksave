#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Test: mgba-lua's `--save <path>` flag loads a sidecar .sav fixture into the
core's save backend before reset, and the loaded bytes are visible from Lua.

End-to-end proof: read the first 16 bytes of `libpksave/tests/fixtures/rs.sav`
directly in Python, run `mgba-lua --save <truncated_fixture>` against the
editor ROM and `probe_sram_first_bytes.lua`, parse the `sidecar: head=...`
line out of stdout, and assert the two byte-sequences match.

The fixture in this branch is 278,520 bytes (a libpksave fixture-pipeline
artefact — another track owns that fix). The editor ROM currently declares
SRAM 32 KiB. To keep this test stable across both the in-flight ROM cart-
header change (FLASH 128 KiB) and the in-flight fixture rewrite, we truncate
to 128 KiB before passing the file in. The first 16 bytes are at offset 0
in every case, so the assertion holds regardless of which save backend the
cart ultimately allocates.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_sidecar_save.py
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile

DOCKER_IMAGE = "pksave-test-gba"

_HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", ".."))

EDITOR_ROM_HOST = os.path.join(REPO_ROOT, "frontend-gba", "pksave-gba.gba")
FIXTURE_HOST = os.path.join(REPO_ROOT, "libpksave", "tests", "fixtures", "rs.sav")
PROBE_LUA_HOST = os.path.join(_HERE, "probe_sram_first_bytes.lua")

EDITOR_ROM_CONTAINER = "/src/frontend-gba/pksave-gba.gba"
PROBE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/probe_sram_first_bytes.lua"

# 128 KiB: the largest GBA save backend (FLASH 128K). Truncating to this size
# is a no-op when mGBA only allocates 32 KiB SRAM — the leading bytes still
# land at offset 0 either way.
TRUNCATE_TO = 128 * 1024
HEAD_LEN = 16
TIMEOUT_SECONDS = 60

HEAD_LINE_RE = re.compile(r"sidecar: head=([0-9a-fA-F]{32})")


def fail(msg, result=None):
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout ---\n{result.stdout}", file=sys.stderr)
        print(f"--- stderr ---\n{result.stderr}", file=sys.stderr)
    sys.exit(1)


def main():
    if not os.path.isfile(EDITOR_ROM_HOST):
        fail(f"editor ROM missing: {EDITOR_ROM_HOST}")
    if not os.path.isfile(FIXTURE_HOST):
        fail(f"fixture missing: {FIXTURE_HOST}")
    if not os.path.isfile(PROBE_LUA_HOST):
        fail(f"probe lua missing: {PROBE_LUA_HOST}")

    with open(FIXTURE_HOST, "rb") as f:
        full_fixture = f.read()
    expected_head = full_fixture[:HEAD_LEN]
    expected_hex = expected_head.hex()

    # Stage a truncated copy of the fixture in a temp dir so we don't mutate
    # the libpksave fixture file and so the container can bind-mount a stable
    # path that doesn't depend on the libpksave fixture rewrite landing first.
    with tempfile.TemporaryDirectory(prefix="pksave-sidecar-") as stage_dir:
        save_host = os.path.join(stage_dir, "rs.sav")
        with open(save_host, "wb") as out:
            out.write(full_fixture[:TRUNCATE_TO])

        save_container = "/save/rs.sav"
        # /save is bound RW because mCoreLoadSaveFile() opens with O_RDWR even
        # in temporary mode (see mGBA src/core/core.c). We pass `temporary=true`
        # at the C level so mGBA still uses an in-memory copy and the file
        # contents are NOT actually written back during the run — but the
        # initial open() still needs write permission to succeed.
        try:
            result = subprocess.run(
                ["docker", "run", "--rm",
                 "-v", f"{REPO_ROOT}:/src:ro",
                 "-v", f"{stage_dir}:/save:rw",
                 DOCKER_IMAGE,
                 "mgba-lua",
                 EDITOR_ROM_CONTAINER,
                 PROBE_LUA_CONTAINER,
                 "10",
                 "--save", save_container],
                capture_output=True, text=True, timeout=TIMEOUT_SECONDS,
            )
        except FileNotFoundError:
            fail("docker not found on PATH")
        except subprocess.TimeoutExpired:
            fail(f"mgba-lua did not exit within {TIMEOUT_SECONDS}s")

    if result.returncode != 0:
        fail(f"mgba-lua exited {result.returncode}", result)
    if "sidecar: OK" not in result.stdout:
        fail("'sidecar: OK' marker not found", result)

    match = HEAD_LINE_RE.search(result.stdout)
    if not match:
        fail("could not parse 'sidecar: head=<hex>' line from stdout", result)
    actual_hex = match.group(1).lower()

    if actual_hex != expected_hex:
        fail(
            "SRAM head bytes do not match fixture\n"
            f"  expected: {expected_hex}\n"
            f"  actual:   {actual_hex}",
            result,
        )

    print(
        f"PASS: sidecar save loaded; first {HEAD_LEN} bytes of SRAM match "
        f"first {HEAD_LEN} bytes of rs.sav: {expected_hex}"
    )
    return 0


if __name__ == "__main__":
    if shutil.which("docker") is None:
        print("SKIP: docker not on PATH", file=sys.stderr)
        sys.exit(0)
    sys.exit(main())
