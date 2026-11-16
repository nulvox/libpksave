#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
"""Verify bin/pksave-gbc.gbc loads as a GBC cartridge in mGBA and runs frames."""

from gbc_harness import fail, run_gbc_lua

LUA = """
callbacks:add("shutdown", function()
    console:log(string.format("gbc_load: frames=%d", h.frame()))
end)
"""


def main():
    _, _, res, _ = run_gbc_lua(LUA, frames=30, fixture_name="rb.sav")
    if "gbc_load: frames=30" not in res.stdout:
        fail("expected 'gbc_load: frames=30' in stdout", res)
    print("OK: bin/pksave-gbc.gbc loaded and ran 30 frames")


if __name__ == "__main__":
    main()
