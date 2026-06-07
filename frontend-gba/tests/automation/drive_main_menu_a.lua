-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- Boot the editor ROM, let the main menu stabilize, tap A once to select
-- the Trainer entry (first of the menu list per
-- docs/frontend-gba-plan.md §"Checkpoint #1"), then dump SRAM in the
-- shutdown callback. Used by gate-I as the negative-control for
-- "navigation does not mutate FLASH".

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

h.tap(h.KEY.A, 30)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("drive: OK")
end)
