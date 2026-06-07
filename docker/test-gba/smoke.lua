-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- Smoke test for the mGBA-Lua harness.
--
-- Proves that:
--   1. The Lua engine is registered and runs the script.
--   2. The `console:log` global writes to stdout via the mGBA logger.
--   3. The `emu` global (mScriptCoreAdapter) is attached to a real core.
--   4. The `callbacks` global can register a per-frame hook that fires
--      while the host advances the emulator.

console:log("smoke: lua engine alive")
console:log(string.format("smoke: platform=%d frame=%d", emu:platform(), emu:currentFrame()))

local frames_seen = 0
callbacks:add("frame", function()
    frames_seen = frames_seen + 1
    if frames_seen == 1 or frames_seen % 30 == 0 then
        console:log(string.format("smoke: frame callback tick #%d emu.frame=%d", frames_seen, emu:currentFrame()))
    end
end)

callbacks:add("shutdown", function()
    console:log(string.format("smoke: shutdown frames_seen=%d emu.frame=%d", frames_seen, emu:currentFrame()))
    console:log("smoke: OK")
end)
