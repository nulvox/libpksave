-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- Dump the entire SRAM/FLASH region the running ROM declared, to /dump/sram.bin.
-- The host mounts /dump as a writable bind so Python can read the result back
-- after the container exits.
--
-- Size detection strategy
-- ───────────────────────
-- mGBA 0.10.5 (the version pinned in docker/test-gba) does NOT register a
-- named `emu.memory.sram` userdata, so we cannot ask the region for its
-- length directly. The verified accessors are:
--   * `emu.memory.sram` — nil in this build (named regions in 0.10.5:
--     bios, iwram, wram, io, palette, vram, oam, cart0/1/2; no sram).
--   * `emu.memory.cart2:size()` — returns the 32 MiB cart-space mapping
--     length, not the SRAM size. Misleading; do not use.
--   * `emu:read8/write8` — work against the GBA bus, hit the SRAM mapping
--     at 0x0E000000 after a warm-up read forces the savegame to allocate.
--
-- Strategy:
--   1. Warm-up read at 0x0E000000 to force mGBA to detect/allocate the
--      savegame (the log emits "Detected SRAM savegame" or "Detected FLASH
--      <N>kB savegame" at this point).
--   2. (Best-effort) try `emu.memory.sram:readRange(0, len)` and any
--      `:size()` / `__len` accessor — guards us against future mGBA
--      versions that DO expose the named region.
--   3. Fallback: detect size by writing a one-byte marker at SRAM[0], then
--      reading back at offsets 0x8000 / 0x10000 / 0x20000 / 0x40000. The
--      lowest offset whose byte equals the marker is where the SRAM region
--      wraps via the GBA's address-mirror behaviour, so it equals the
--      region size (32 KiB / 64 KiB / 128 KiB respectively). Restore the
--      original byte afterwards.
--   4. Dump `region_len` bytes via a tight `emu:read8` loop and write the
--      blob to /dump/sram.bin via io.open(...,"wb"):write.
--
-- The Python side parses `dump_full: size=N` from stdout and asserts the
-- on-disk file matches.

local DUMP_PATH = "/dump/sram.bin"
local SRAM_BASE = 0x0E000000

console:log("dump_full: lua engine alive")

-- 1. Warm-up — forces mGBA to allocate the savegame region. After this point
--    emu:read8(SRAM_BASE+i) reads real SRAM/FLASH bytes (0xFF for fresh).
local first = emu:read8(SRAM_BASE)
console:log(string.format("dump_full: warm-up sram[0]=0x%02x", first))

-- 2. Best-effort named-region path. Guarded against absence (mGBA 0.10.5),
--    against `:size()` returning the cart-space mapping size (always 32 MiB
--    for cart0/1/2), and against `:readRange` returning something that isn't
--    a Lua string of the requested length.
local function try_named_region()
    local r = emu.memory and emu.memory.sram
    if r == nil then return nil end
    -- Try common length accessors.
    local len = nil
    for _, fname in ipairs({"size", "length", "len"}) do
        local ok, v = pcall(function() return r[fname](r) end)
        if ok and type(v) == "number" and v > 0 and v <= 1024 * 1024 then
            -- Sanity gate: GBA SRAM/FLASH is 32/64/128 KiB; cart-space maps
            -- are 32 MiB so any number > 1 MiB is the misleading one.
            len = v
            console:log(string.format("dump_full: named-region :%s() = %d", fname, len))
            break
        end
    end
    if not len then
        local ok, v = pcall(function() return #r end)
        if ok and type(v) == "number" then
            len = v
            console:log(string.format("dump_full: named-region #r = %d", len))
        end
    end
    if not len then return nil end
    local ok, blob = pcall(function() return r:readRange(0, len) end)
    if ok and type(blob) == "string" and #blob == len then
        return blob, len, "named-region"
    end
    return nil
end

-- 3. Mirror-based size detection via emu:read8/write8.
local function detect_size_via_mirror()
    local orig = emu:read8(SRAM_BASE)
    -- Pick a marker that differs from orig so the test is sound even if SRAM
    -- happens to be all 0x55.
    local marker = (orig == 0x55) and 0xAA or 0x55
    emu:write8(SRAM_BASE, marker)
    local readback = emu:read8(SRAM_BASE)
    if readback ~= marker then
        -- Should not happen on GBA SRAM/FLASH after warm-up, but if it does
        -- (e.g., a write-protected region), restore and bail out.
        emu:write8(SRAM_BASE, orig)
        return nil
    end

    local detected = nil
    -- The lowest mirror offset is the region size.
    for _, sz in ipairs({32768, 65536, 131072}) do
        local v = emu:read8(SRAM_BASE + sz)
        if v == marker then
            detected = sz
            break
        end
    end

    -- Restore original byte to avoid leaving the savegame dirty.
    emu:write8(SRAM_BASE, orig)
    return detected
end

local blob, region_len, source

blob, region_len, source = try_named_region()
if blob == nil then
    region_len = detect_size_via_mirror()
    if region_len == nil then
        console:log("dump_full: ERROR — could not detect region size via mirror probe")
        os.exit(1)
    end
    source = "emu:read8 loop"
    console:log(string.format("dump_full: detected region_len=%d via mirror probe", region_len))

    -- 4. Bulk dump via emu:read8 loop. For 128 KiB this is ~131k Lua calls;
    --    measured ~0.5–1.5s on the harness host, well inside the test budget.
    local bytes = {}
    for i = 0, region_len - 1 do
        bytes[i + 1] = string.char(emu:read8(SRAM_BASE + i))
    end
    blob = table.concat(bytes)
else
    console:log(string.format("dump_full: read region via named-region API len=%d", region_len))
end

if #blob ~= region_len then
    console:log(string.format("dump_full: ERROR — blob length %d != region_len %d", #blob, region_len))
    os.exit(1)
end

local f, err = io.open(DUMP_PATH, "wb")
if not f then
    console:log(string.format("dump_full: io.open(%s) failed: %s", DUMP_PATH, tostring(err)))
    os.exit(1)
end
f:write(blob)
f:close()

console:log(string.format("dump_full: source=%s", source))
console:log(string.format("dump_full: size=%d", #blob))
console:log(string.format("dump_full: wrote %d bytes to %s", #blob, DUMP_PATH))
console:log("dump_full: OK")
