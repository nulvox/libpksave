-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- CP#4 nav test: drive bag_scene navigation without mutating SRAM.
--
-- Sequence:
--   Frame  30 — DOWN×2 from main_menu cursor (Trainer=0) to Bag (=2).
--      60 — DOWN
--      90 — A    — into bag_scene (ITEMS pocket selected).  Logs "bag: OK".
--     120 — RIGHT — pocket index 0 → 1 (BALLS).  Triggers load_pocket().
--     150 — RIGHT — 1 → 2 (TM/HM).
--     180 — LEFT  — 2 → 1 (BALLS).
--     210 — B    — back to main_menu.
--   shutdown — dump SRAM (expect all 0xFF — no FLASH writes).
--
-- No A on a slot, no Save & Exit — proves nav alone produces zero FLASH
-- writes (same negative-control pattern as test_trainer_scene_nav.py).

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

h.tap(h.KEY.DOWN,  30)
h.tap(h.KEY.DOWN,  60)
h.tap(h.KEY.A,     90)
h.tap(h.KEY.RIGHT, 120)
h.tap(h.KEY.RIGHT, 150)
h.tap(h.KEY.LEFT,  180)
h.tap(h.KEY.B,     210)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("bag_nav: OK")
end)
