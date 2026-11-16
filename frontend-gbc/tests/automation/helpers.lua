-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 nulvox
--
-- Shared Lua primitives for the frontend-gbc mGBA-Lua test harness.

local M = {}

M.KEY = {
    A      = 0x0001,
    B      = 0x0002,
    SELECT = 0x0004,
    START  = 0x0008,
    RIGHT  = 0x0010,
    LEFT   = 0x0020,
    UP     = 0x0040,
    DOWN   = 0x0080,
}

local _frame_count = 0
local _queue = {}

callbacks:add("frame", function()
    _frame_count = _frame_count + 1
    for _, entry in ipairs(_queue) do
        if entry.frame == _frame_count then
            entry.fn(_frame_count)
        end
    end
end)

function M.at(frame, fn)
    table.insert(_queue, {frame = frame, fn = fn})
end

function M.frame()
    return _frame_count
end

local TAP_HOLD_DEFAULT = 3

function M.tap(key, frame, hold)
    hold = hold or TAP_HOLD_DEFAULT
    M.at(frame,        function() emu:setKeys(key) end)
    M.at(frame + hold, function() emu:setKeys(0) end)
end

function M.sram_read_all()
    local sram = emu.memory.sram
    if not sram then
        console:log("helpers.sram_read_all: emu.memory.sram not registered")
        return "", 0
    end
    local sz = sram:size()
    local bytes = {}
    for i = 0, sz - 1 do
        bytes[#bytes + 1] = string.char(sram:read8(i))
    end
    return table.concat(bytes), sz
end

function M.sram_dump(path)
    path = path or "/dump/sram.bin"
    local blob, sz = M.sram_read_all()
    local f, err = io.open(path, "wb")
    if not f then
        console:log(string.format("helpers.sram_dump: io.open(%s) failed: %s", path, tostring(err)))
        return 0
    end
    f:write(blob)
    f:close()
    console:log(string.format("helpers.sram_dump: wrote %d bytes (region=%d) to %s",
                              #blob, sz, path))
    return #blob
end

function M.fb_dump(path)
    path = path or "/dump/fb.ppm"
    if not fb_dump_ppm then
        console:log("helpers.fb_dump: fb_dump_ppm global not registered")
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
