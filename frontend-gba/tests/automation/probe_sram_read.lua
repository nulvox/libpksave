-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- Probe: enumerate mGBA's Lua memory accessors and find one that maps to SRAM.

console:log("probe: lua engine alive")

if not emu or not emu.memory then
    console:log("probe: emu.memory missing")
    os.exit(1)
end

local mem = emu.memory
console:log(string.format("probe: emu.memory type=%s", type(mem)))

-- All known/plausible names from mGBA source (src/gba/scripting.c registers
-- bios, ewram, iwram, io, palette, vram, oam, cart0, cart1, cart2, sram).
-- Probe lowercase, uppercase, and a few variants.
local candidates = {
    "sram", "SRAM", "Sram",
    "ewram", "EWRAM", "Ewram",
    "cart0", "cart1", "cart2",
    "iwram", "bios", "vram", "oam", "io", "palette",
    "save", "flash", "battery",
}
for _, name in ipairs(candidates) do
    local r = mem[name]
    if r ~= nil then
        console:log(string.format("probe:   mem.%-10s = %s", name, tostring(r)))
    end
end

-- Try absolute-address reads via emu:read8 if it exists (GBA SRAM lives at
-- 0x0E000000). This bypasses the named-region machinery entirely.
if emu.read8 then
    console:log("probe: emu:read8 exists, attempting absolute reads")
    for _, info in ipairs({
        {addr=0x08000000, label="cart0[0] (ROM)"},
        {addr=0x0E000000, label="sram[0]"},
        {addr=0x02000000, label="ewram[0]"},
    }) do
        local ok, val = pcall(function() return emu:read8(info.addr) end)
        if ok then
            console:log(string.format("probe:   emu:read8(0x%08x)=0x%02x  -- %s", info.addr, val, info.label))
        else
            console:log(string.format("probe:   emu:read8(0x%08x) failed: %s  -- %s", info.addr, tostring(val), info.label))
        end
    end
else
    console:log("probe: emu.read8 not available")
end

-- Inspect metatable of mem to see what methods/fields exist.
local mt = getmetatable(mem)
if mt then
    console:log("probe: emu.memory metatable fields:")
    for k, _ in pairs(mt) do
        console:log(string.format("probe:   mt[%s]", tostring(k)))
    end
    if mt.__index and type(mt.__index) == "table" then
        console:log("probe: emu.memory __index table fields:")
        for k, _ in pairs(mt.__index) do
            console:log(string.format("probe:   __index[%s]", tostring(k)))
        end
    end
else
    console:log("probe: emu.memory has no metatable")
end

console:log("probe: done")
