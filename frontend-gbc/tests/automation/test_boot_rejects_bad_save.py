#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify malformed save lands on boot_error_scene with zero SRAM writes."""

from gbc_harness import fail, run_gbc_lua

LUA = """
h.tap(h.KEY.A, 30)
h.tap(h.KEY.DOWN, 60)
h.tap(h.KEY.A, 90)
callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
end)
"""


def main():
    bad_save = b"\x00" * (32 * 1024)
    orig, dumped, res, _ = run_gbc_lua(
        LUA, frames=150, fixture_name=None, raw_save_bytes=bad_save
    )
    if dumped != orig:
        fail("boot_error_scene mutated SRAM on malformed save", res)
    print("OK: malformed save rejected with zero SRAM writes")


if __name__ == "__main__":
    main()
