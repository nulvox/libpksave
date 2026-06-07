-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- Probe: enumerate mGBA's Lua input API surface. We don't know which of
-- emu:setKeys / emu:addKeys / emu:clearKeys / emu.input / C_KEY_* are
-- actually exposed in mGBA 0.10.5 against the GBA core, so this script
-- prints whatever it can find and tries each candidate inside a pcall.
--
-- Pattern after probe_sram_read.lua.

console:log("probe: lua engine alive")

if not emu then
    console:log("probe: emu missing")
    os.exit(1)
end

-- ── 1. Enumerate top-level globals that look input-related ──────────────────
console:log("probe: scanning _G for input-related globals")
for k, v in pairs(_G) do
    local kl = string.lower(tostring(k))
    if string.find(kl, "key") or string.find(kl, "input") or string.find(kl, "button") or string.find(kl, "pad") then
        console:log(string.format("probe:   _G[%s] = %s", tostring(k), tostring(v)))
    end
end

-- ── 2. Enumerate emu's metatable __index for input-related methods ──────────
local emu_mt = getmetatable(emu)
if emu_mt then
    console:log("probe: emu metatable fields:")
    for k, _ in pairs(emu_mt) do
        console:log(string.format("probe:   emu_mt[%s]", tostring(k)))
    end
    if emu_mt.__index and type(emu_mt.__index) == "table" then
        console:log("probe: emu __index keys (full list):")
        local names = {}
        for k, _ in pairs(emu_mt.__index) do
            names[#names + 1] = tostring(k)
        end
        table.sort(names)
        for _, n in ipairs(names) do
            console:log(string.format("probe:   emu.%s", n))
        end
    end
end

-- ── 3. Check for input-related fields/methods on emu directly ───────────────
local probe_methods = {
    "setKeys", "addKeys", "clearKeys",
    "setKey", "addKey", "clearKey",
    "keys", "input", "joypad", "pad",
}
console:log("probe: direct emu.<name> probes")
for _, name in ipairs(probe_methods) do
    local v = emu[name]
    console:log(string.format("probe:   emu.%-12s = %s", name, tostring(v)))
end

-- ── 4. If emu.input exists, enumerate its metatable too ────────────────────
if emu.input then
    console:log("probe: emu.input exists, type=" .. type(emu.input))
    local imt = getmetatable(emu.input)
    if imt and imt.__index and type(imt.__index) == "table" then
        console:log("probe: emu.input __index keys:")
        for k, _ in pairs(imt.__index) do
            console:log(string.format("probe:   emu.input.%s", tostring(k)))
        end
    end
end

-- ── 5. Look for C_KEY_* and KEY_* constants ────────────────────────────────
console:log("probe: scanning _G for KEY constants")
local key_consts = {}
for k, v in pairs(_G) do
    local ks = tostring(k)
    if string.find(ks, "KEY") or string.find(ks, "C_KEY") then
        key_consts[#key_consts + 1] = {name=ks, val=v}
    end
end
table.sort(key_consts, function(a, b) return a.name < b.name end)
for _, kc in ipairs(key_consts) do
    console:log(string.format("probe:   _G[%s] = %s", kc.name, tostring(kc.val)))
end

-- ── 6. Look for a C global (mGBA exposes some enum constants via C.*) ──────
if _G["C"] then
    console:log("probe: C global exists, type=" .. type(_G["C"]))
    if type(_G["C"]) == "table" or type(_G["C"]) == "userdata" then
        local cmt = getmetatable(_G["C"])
        if cmt and cmt.__index and type(cmt.__index) == "table" then
            console:log("probe: C __index keys:")
            local names = {}
            for k, _ in pairs(cmt.__index) do
                names[#names + 1] = tostring(k)
            end
            table.sort(names)
            for _, n in ipairs(names) do
                console:log(string.format("probe:   C.%s", n))
            end
        else
            -- raw table
            local names = {}
            for k, _ in pairs(_G["C"]) do
                names[#names + 1] = tostring(k)
            end
            table.sort(names)
            for _, n in ipairs(names) do
                console:log(string.format("probe:   C.%s", n))
            end
        end
    end
end

-- ── 6b. Drill into C.GBA_KEY since the previous step said it exists ────────
if _G["C"] and _G["C"]["GBA_KEY"] then
    local gk = _G["C"]["GBA_KEY"]
    console:log("probe: C.GBA_KEY exists, type=" .. type(gk))
    local gkmt = getmetatable(gk)
    if gkmt and gkmt.__index and type(gkmt.__index) == "table" then
        console:log("probe: C.GBA_KEY __index keys:")
        local names = {}
        for k, _ in pairs(gkmt.__index) do names[#names + 1] = tostring(k) end
        table.sort(names)
        for _, n in ipairs(names) do
            local v = gk[n]
            console:log(string.format("probe:   C.GBA_KEY.%-8s = %s", n, tostring(v)))
        end
    else
        -- Try common name access; some bindings make enums non-iterable.
        for _, n in ipairs({"A","B","SELECT","START","RIGHT","LEFT","UP","DOWN","R","L"}) do
            local ok, v = pcall(function() return gk[n] end)
            if ok and v ~= nil then
                console:log(string.format("probe:   C.GBA_KEY.%-8s = %s", n, tostring(v)))
            end
        end
    end
end

-- ── 7. Try calling each candidate inside a pcall to see if it accepts args ─
local KEYINPUT = 0x04000130
local function read_ki()
    return emu:read16(KEYINPUT)
end

local function trycall(label, fn)
    local ok, err = pcall(fn)
    console:log(string.format("probe: %-32s ok=%s err=%s", label, tostring(ok), tostring(err)))
end

-- baseline
console:log(string.format("probe: KEYINPUT idle=0x%04x (active-low; expect 0x03ff)", read_ki()))

if emu.setKeys then
    trycall("emu:setKeys(0x0001) -- A", function() emu:setKeys(0x0001) end)
    console:log(string.format("probe: KEYINPUT after setKeys(1)=0x%04x", read_ki()))
    trycall("emu:setKeys(0)", function() emu:setKeys(0) end)
    console:log(string.format("probe: KEYINPUT after setKeys(0)=0x%04x", read_ki()))
end

if emu.addKeys then
    trycall("emu:addKeys(0x0001)", function() emu:addKeys(0x0001) end)
    console:log(string.format("probe: KEYINPUT after addKeys(1)=0x%04x", read_ki()))
end

if emu.clearKeys then
    trycall("emu:clearKeys(0x0001)", function() emu:clearKeys(0x0001) end)
    console:log(string.format("probe: KEYINPUT after clearKeys(1)=0x%04x", read_ki()))
end

console:log("probe: done")
