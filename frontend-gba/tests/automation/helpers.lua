-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- Shared Lua primitives for the mGBA-Lua test harness.
-- `require` it from a test script: `local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")`
-- (mgba-lua's Lua engine doesn't set up package.path, so dofile is the
-- portable include mechanism.)
--
-- Centralising input/timing/dump primitives here means future tuning
-- (frame-timing tweaks, butano debounce changes) happens in one place
-- rather than spread across N test scripts.

local M = {}

-- ── GBA constants ───────────────────────────────────────────────────────────
M.SRAM_BASE = 0x0E000000

-- KEYINPUT bit ordering, matching docker/test-gba/probe_keys.lua findings.
M.KEY = {
    A      = 0x0001,
    B      = 0x0002,
    SELECT = 0x0004,
    START  = 0x0008,
    RIGHT  = 0x0010,
    LEFT   = 0x0020,
    UP     = 0x0040,
    DOWN   = 0x0080,
    R      = 0x0100,
    L      = 0x0200,
}

-- ── Frame scheduling ────────────────────────────────────────────────────────
--
-- Tests schedule actions at specific frame numbers via M.at(frame, fn).
-- Internally we maintain one global frame callback that dispatches all
-- queued entries. This avoids each test having to wire its own counter.

local _frame_count = 0
local _queue = {}  -- list of {frame=N, fn=function}

callbacks:add("frame", function()
    _frame_count = _frame_count + 1
    for _, entry in ipairs(_queue) do
        if entry.frame == _frame_count then
            entry.fn(_frame_count)
        end
    end
end)

--- Run `fn` exactly once on the frame whose count equals `frame`.
-- @param frame integer  1-indexed frame count (first frame after run starts = 1)
-- @param fn    function called with the frame index.
function M.at(frame, fn)
    table.insert(_queue, {frame = frame, fn = fn})
end

--- Current frame index (since run start).
function M.frame()
    return _frame_count
end

-- ── Input ───────────────────────────────────────────────────────────────────
--
-- `tap(key, frame, hold)` schedules a press-on-frame, release-on-frame+hold
-- pair. Default hold = 3 frames (clears butano's input debounce per the
-- empirical findings in test_key_input.py).

local TAP_HOLD_DEFAULT = 3

function M.tap(key, frame, hold)
    hold = hold or TAP_HOLD_DEFAULT
    M.at(frame,        function() emu:setKeys(key) end)
    M.at(frame + hold, function() emu:setKeys(0) end)
end

-- ── SRAM/FLASH dump ─────────────────────────────────────────────────────────
--
-- Detect the allocated save region size by writing a marker byte and
-- looking for the GBA address-mirror wrap point. Necessary because
-- mGBA 0.10.5 does NOT register emu.memory.sram by name (per Track E's
-- probe), so we can't ask the region directly.

local function mirror_probe_size(base, first_byte)
    local marker = (first_byte + 1) % 256
    emu:write8(base, marker)
    local detected = nil
    for _, size in ipairs({0x8000, 0x10000, 0x20000}) do
        if emu:read8(base + size) == marker then
            detected = size
            break
        end
    end
    emu:write8(base, first_byte)
    return detected or 0x20000
end

--- Read the entire currently-allocated SRAM/FLASH region into a string.
-- Returns (blob, region_len).
function M.sram_read_all()
    local base = M.SRAM_BASE
    local first = emu:read8(base)
    local region_len = mirror_probe_size(base, first)
    local bytes = {}
    for i = 0, region_len - 1 do
        bytes[#bytes + 1] = string.char(emu:read8(base + i))
    end
    return table.concat(bytes), region_len
end

--- Dump the SRAM/FLASH region to a file (default /dump/sram.bin).
-- Returns the number of bytes written.
function M.sram_dump(path)
    path = path or "/dump/sram.bin"
    local blob, region_len = M.sram_read_all()
    local f, err = io.open(path, "wb")
    if not f then
        console:log(string.format("helpers.sram_dump: io.open(%s) failed: %s", path, tostring(err)))
        return 0
    end
    f:write(blob)
    f:close()
    console:log(string.format("helpers.sram_dump: wrote %d bytes (region=%d) to %s",
                              #blob, region_len, path))
    return #blob
end

-- ── Framebuffer dump ────────────────────────────────────────────────────────
--
-- `fb_dump(path)` writes the current 240×160 GBA framebuffer as a binary
-- PPM (P6) file. Backed by the custom `fb_dump_ppm` Lua global registered
-- in docker/test-gba/mgba-lua.c (see its file header). PPM was picked over
-- PNG so the harness's Python helpers can parse with stdlib only (no PIL).
--
-- mgba's `emu:screenshot()` also works (writes PNG) now that mgba-lua.c
-- wires up a video buffer at startup, but the Python side has no PNG
-- decoder; reach for PPM via fb_dump.
--
-- Returns true on success and surfaces the C-side error code on failure
-- so the calling test can fail loudly rather than silently dumping zero
-- bytes.

function M.fb_dump(path)
    path = path or "/dump/fb.ppm"
    if not fb_dump_ppm then
        console:log("helpers.fb_dump: fb_dump_ppm global not registered — " ..
                    "mgba-lua.c framebuffer support missing or harness rebuild needed")
        return false
    end
    local rc = fb_dump_ppm(path)
    if rc ~= 0 then
        console:log(string.format("helpers.fb_dump: fb_dump_ppm(%s) returned %d", path, rc))
        return false
    end
    console:log(string.format("helpers.fb_dump: wrote PPM to %s", path))
    return true
end

return M
