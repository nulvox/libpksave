#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Host-side unit tests for widgets/cycler — compiled and run on the host
with the system g++.  No docker, no emulator; the cycler's input layer
goes through widgets/keypad_input.hpp which the test replaces with a
settable stub (tests/host/keypad_input_host.cpp).

This is faster than the lua-driven harness tests and gives a tight TDD
loop for pure-logic widgets.  Pattern can be reused for future widgets.
"""

import os
import subprocess
import sys
import tempfile

_HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", ".."))

CXX = os.environ.get("CXX", "g++")
SRC_DIR  = os.path.join(REPO_ROOT, "frontend-gba", "src")
HOST_DIR = os.path.join(REPO_ROOT, "frontend-gba", "tests", "host")

SOURCES = [
    os.path.join(SRC_DIR, "widgets", "cycler.cpp"),
    os.path.join(HOST_DIR, "keypad_input_host.cpp"),
    os.path.join(HOST_DIR, "test_cycler.cpp"),
]
INCLUDES = ["-I" + os.path.join(SRC_DIR, "widgets")]


def main() -> int:
    for s in SOURCES:
        if not os.path.isfile(s):
            print(f"FAIL: source missing: {s}", file=sys.stderr)
            return 1

    with tempfile.TemporaryDirectory(prefix="cycler-host-") as td:
        binary = os.path.join(td, "test_cycler")
        cmd = [CXX, "-std=c++20", "-Wall", "-Wextra", "-O0", "-g",
               *INCLUDES, *SOURCES, "-o", binary]
        cp = subprocess.run(cmd, capture_output=True, text=True)
        if cp.returncode != 0:
            print("FAIL: compile failed", file=sys.stderr)
            print(cp.stderr, file=sys.stderr)
            return 1
        run = subprocess.run([binary], capture_output=True, text=True)
        sys.stdout.write(run.stdout)
        sys.stderr.write(run.stderr)
        if run.returncode != 0:
            print("FAIL: test binary exited non-zero", file=sys.stderr)
            return 1
    print("PASS: cycler host tests")
    return 0


if __name__ == "__main__":
    sys.exit(main())
