-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- tap_a_probe: prove that a single A-button tap driven from Lua is
-- observable inside the GBA core via the KEYINPUT register (0x04000130).
--
-- Sequence (1-based frame counter inside this script):
--   frame 1..2: no keys held         -> KEYINPUT bit 0 == 1 (idle)
--   frame 3   : emu:setKeys(KEY_A)   -> KEYINPUT bit 0 == 0 (pressed)
--   frame 4   : emu:setKeys(0)       -> KEYINPUT bit 0 == 1 (released)
--
-- The assertion is done in Lua at shutdown so a failure aborts the
-- container with a non-zero exit code; the Python wrapper also greps
-- the per-frame log for redundant verification.

local KEYINPUT = 0x04000130
local KEY_A_MASK = 0x0001 -- bit 0 of KEYCNT/KEYINPUT == A

-- Frames at which we set / release A. We deliberately leave a few idle
-- frames before pressing so the "baseline = unpressed" assertion has
-- multiple observations.
local FRAME_PRESS   = 3
local FRAME_RELEASE = 4
local FRAME_DONE    = 6

console:log("tapA: lua engine alive")
console:log(string.format("tapA: KEY_A_MASK=0x%04x  KEYINPUT=0x%08x", KEY_A_MASK, KEYINPUT))

if not emu.setKeys then
    console:log("tapA: FAIL emu:setKeys not exposed by this mGBA build")
    os.exit(2)
end

local samples = {}   -- per-frame KEYINPUT log: samples[frame] = value
local frame = 0

callbacks:add("frame", function()
    frame = frame + 1

    -- Apply input change at the start of the frame so the core sees it
    -- this iteration. mGBA's KEYINPUT register snapshots whatever was
    -- last fed via setKeys/addKeys regardless of frame boundary, but
    -- ordering this way keeps the semantics readable.
    if frame == FRAME_PRESS then
        emu:setKeys(KEY_A_MASK)
    elseif frame == FRAME_RELEASE then
        emu:setKeys(0)
    end

    local ki = emu:read16(KEYINPUT)
    samples[frame] = ki
    console:log(string.format("tapA: frame=%d KEYINPUT=0x%04x bitA=%d", frame, ki, ki & 1))

    if frame >= FRAME_DONE then
        -- We've collected enough; the host will fire shutdown next.
        -- Just stop logging more frames.
    end
end)

local function assert_eq(label, got, want)
    if got == want then
        console:log(string.format("tapA: PASS %s got=0x%04x want=0x%04x", label, got, want))
        return true
    else
        console:log(string.format("tapA: FAIL %s got=0x%04x want=0x%04x", label, got, want))
        return false
    end
end

callbacks:add("shutdown", function()
    console:log(string.format("tapA: shutdown after %d frames", frame))
    local ok = true

    -- Baseline: bit 0 of KEYINPUT is set (= A unpressed) on the idle frames.
    for _, f in ipairs({1, 2}) do
        local v = samples[f]
        if v == nil then
            console:log(string.format("tapA: FAIL no sample for idle frame %d", f))
            ok = false
        else
            -- All 10 GBA key bits should be set (idle), i.e. low 10 bits == 0x3ff.
            if (v & 0x3ff) ~= 0x3ff then
                console:log(string.format("tapA: FAIL frame %d idle KEYINPUT=0x%04x (expected low10==0x3ff)", f, v))
                ok = false
            else
                console:log(string.format("tapA: ok frame %d idle KEYINPUT=0x%04x", f, v))
            end
        end
    end

    -- Pressed frame: bit 0 cleared, the other 9 bits still set.
    local vp = samples[FRAME_PRESS]
    if vp == nil then
        console:log("tapA: FAIL no sample for press frame")
        ok = false
    else
        if (vp & 0x001) ~= 0 then
            console:log(string.format("tapA: FAIL press frame KEYINPUT=0x%04x bit0 not cleared", vp))
            ok = false
        elseif (vp & 0x3fe) ~= 0x3fe then
            console:log(string.format("tapA: FAIL press frame other keys not idle KEYINPUT=0x%04x", vp))
            ok = false
        else
            console:log(string.format("tapA: ok press  KEYINPUT=0x%04x (A held)", vp))
        end
    end

    -- Release frame: bit 0 set again.
    local vr = samples[FRAME_RELEASE]
    if vr == nil then
        console:log("tapA: FAIL no sample for release frame")
        ok = false
    else
        if (vr & 0x3ff) ~= 0x3ff then
            console:log(string.format("tapA: FAIL release frame KEYINPUT=0x%04x (expected 0x3ff)", vr))
            ok = false
        else
            console:log(string.format("tapA: ok release KEYINPUT=0x%04x (A released)", vr))
        end
    end

    if ok then
        console:log("tapA: OK")
    else
        console:log("tapA: FAIL")
        -- Force non-zero exit from the container so the Python harness
        -- can detect failure even before reading stdout.
        os.exit(1)
    end
end)
