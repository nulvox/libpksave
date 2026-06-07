-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- Dump the full GBA SRAM/FLASH region after the editor's boot code has
-- had a chance to issue its FLASH-unlock command. Doing this in the
-- `shutdown` callback guarantees we run AFTER all frames have advanced
-- and mGBA's lazy save-type autodetect has resolved.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("dump_after_boot: OK")
end)
