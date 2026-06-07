-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- Dump the first DUMP_LEN bytes of GBA SRAM (0x0E000000+) to /dump/sram.bin.
-- The host mounts /dump as a writable bind so Python can read the result
-- back after the container exits.

local DUMP_PATH = "/dump/sram.bin"
local DUMP_LEN  = 16
local SRAM_BASE = 0x0E000000

console:log("dump: lua engine alive")

-- Touch SRAM to force mGBA to allocate the region. After this, both
-- emu:read8(addr) and emu.memory.sram should work.
local first = emu:read8(SRAM_BASE)
console:log(string.format("dump: warm-up read sram[0]=0x%02x", first))

local bytes = {}
for i = 0, DUMP_LEN - 1 do
    bytes[#bytes + 1] = string.char(emu:read8(SRAM_BASE + i))
end
local blob = table.concat(bytes)

local f, err = io.open(DUMP_PATH, "wb")
if not f then
    console:log(string.format("dump: io.open(%s) failed: %s", DUMP_PATH, tostring(err)))
    os.exit(1)
end
f:write(blob)
f:close()

console:log(string.format("dump: wrote %d bytes to %s", #blob, DUMP_PATH))
console:log("dump: OK")
