# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""
Shared test runner utilities for frontend-gbc mGBA-Lua automation tests.
"""

import ctypes
import os
import shutil
import subprocess
import sys
import tempfile

_HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", ".."))
_GBA_AUTO = os.path.join(REPO_ROOT, "frontend-gba", "tests", "automation")
if _GBA_AUTO not in sys.path:
    sys.path.insert(0, _GBA_AUTO)

import pksave_ctypes as pk  # noqa: E402

DOCKER_IMAGE = "pksave-test-gba"
EDITOR_ROM_HOST = os.path.join(REPO_ROOT, "bin", "pksave-gbc.gbc")
EDITOR_ROM_CONTAINER = "/src/bin/pksave-gbc.gbc"
HELPERS_LUA_CONTAINER = "/src/frontend-gbc/tests/automation/helpers.lua"
TIMEOUT_SECONDS = 180

ALL_CASES = [
    ("rb.sav",      pk.Game.RB),
    ("yellow.sav",  pk.Game.YELLOW),
    ("gs.sav",      pk.Game.GS),
    ("crystal.sav", pk.Game.CRYSTAL),
]

GEN2_CASES = [
    ("gs.sav",      pk.Game.GS),
    ("crystal.sav", pk.Game.CRYSTAL),
]


def fail(msg: str, result=None) -> None:
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout (tail) ---\n{result.stdout[-2000:]}", file=sys.stderr)
        print(f"--- stderr (tail) ---\n{result.stderr[-1000:]}", file=sys.stderr)
    sys.exit(1)


def load_lib():
    lib = pk.load_library()
    lib.pksave_validate_checksum.argtypes = [
        ctypes.c_char_p,
        ctypes.c_size_t,
        ctypes.c_int,
    ]
    lib.pksave_validate_checksum.restype = ctypes.c_int
    return lib


def validate_checksum(lib, sram_bytes: bytes, game: pk.Game) -> int:
    return int(lib.pksave_validate_checksum(sram_bytes, len(sram_bytes), int(game)))


def run_gbc_lua(
    lua_body: str,
    frames: int = 420,
    fixture_name: str | None = "rb.sav",
    raw_save_bytes: bytes | None = None,
    extra_dumps: list[str] | None = None,
) -> tuple[bytes, bytes, subprocess.CompletedProcess, dict[str, bytes]]:
    """Run pksave-gbc.gbc under mgba-lua with an inline Lua script.

    Returns (original_save_bytes, dumped_sram_bytes, completed_process, extra_files).
    """
    if not os.path.isfile(EDITOR_ROM_HOST):
        fail(f"missing ROM: {EDITOR_ROM_HOST} (run `make gbc` first)")

    with tempfile.TemporaryDirectory(prefix="pksave-gbc-") as workdir:
        dump_dir = os.path.join(workdir, "dump")
        save_dir = os.path.join(workdir, "save")
        os.makedirs(dump_dir)
        os.makedirs(save_dir)

        script_path = os.path.join(dump_dir, "drive.lua")
        with open(script_path, "w", encoding="utf-8") as f:
            f.write(f'local h = dofile("{HELPERS_LUA_CONTAINER}")\n')
            f.write(lua_body)

        cmd = [
            "docker", "run", "--rm",
            "-v", f"{REPO_ROOT}:/src:ro",
            "-v", f"{save_dir}:/save:rw",
            "-v", f"{dump_dir}:/dump:rw",
            DOCKER_IMAGE,
            "mgba-lua",
            EDITOR_ROM_CONTAINER,
            "/dump/drive.lua",
            str(frames),
        ]

        orig_bytes = b""
        if raw_save_bytes is not None:
            save_path = os.path.join(save_dir, "custom.sav")
            with open(save_path, "wb") as f:
                f.write(raw_save_bytes)
            orig_bytes = raw_save_bytes
            cmd.extend(["--save", "/save/custom.sav"])
        elif fixture_name is not None:
            fixture_src = os.path.join(
                REPO_ROOT, "libpksave", "tests", "fixtures", fixture_name
            )
            if not os.path.isfile(fixture_src):
                fail(f"fixture missing: {fixture_src}")
            with open(fixture_src, "rb") as f:
                orig_bytes = f.read()
            save_path = os.path.join(save_dir, fixture_name)
            shutil.copy2(fixture_src, save_path)
            cmd.extend(["--save", f"/save/{fixture_name}"])

        try:
            result = subprocess.run(
                cmd, capture_output=True, text=True, timeout=TIMEOUT_SECONDS
            )
        except FileNotFoundError:
            fail("docker not found on PATH")
        except subprocess.TimeoutExpired:
            fail(f"mgba-lua timed out after {TIMEOUT_SECONDS}s")

        if result.returncode != 0:
            fail(f"mgba-lua exited {result.returncode}", result)

        sram_file = os.path.join(dump_dir, "sram.bin")
        dumped = b""
        if os.path.isfile(sram_file):
            with open(sram_file, "rb") as f:
                dumped = f.read()

        extras: dict[str, bytes] = {}
        for name in extra_dumps or []:
            p = os.path.join(dump_dir, name)
            if os.path.isfile(p):
                with open(p, "rb") as f:
                    extras[name] = f.read()

        return orig_bytes, dumped, result, extras
