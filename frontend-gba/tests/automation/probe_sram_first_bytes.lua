-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- Read the first 16 bytes of GBA SRAM (mapped at 0x0E000000) and log them
-- as a stable hex line. Paired with test_sidecar_save.py, which loads a
-- sidecar .sav fixture via `mgba-lua --save` and asserts the bytes here
-- match the first 16 bytes of that fixture.
--
-- The warm-up read forces mGBA to materialise the SRAM region before we
-- walk it byte-by-byte — without it the first read can return 0x00 because
-- the backing allocation hasn't been touched yet on a fresh boot.

local READ_LEN  = 16
local SRAM_BASE = 0x0E000000

console:log("sidecar: lua engine alive")

-- Warm-up: trigger SRAM allocation by reading a single byte.
local warm = emu:read8(SRAM_BASE)
console:log(string.format("sidecar: warm-up sram[0]=0x%02x", warm))

local hex = {}
for i = 0, READ_LEN - 1 do
    hex[#hex + 1] = string.format("%02x", emu:read8(SRAM_BASE + i))
end
console:log(string.format("sidecar: head=%s", table.concat(hex)))
console:log("sidecar: OK")
