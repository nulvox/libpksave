-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- CP#5 nav test: drive box_scene navigation without mutating SRAM.
--
-- Sequence:
--   Frame  30 / 60 / 90 — DOWN×3 from main_menu (Trainer=0) to Boxes (=3).
--          130          — A     — into box_scene at box 0.  Logs "box: OK".
--          170          — SELECT — box 0 → 1 (load_box; redraw_header).
--          210          — SELECT — 1 → 2.
--          250          — SELECT — 2 → 3.
--          290          — B     — back to main_menu.
--   shutdown — dump SRAM (expect identical to the sidecar fixture: no
--              FLASH writes during nav).
--
-- No A on a slot, no Save & Exit.  Mirrors test_bag_scene_nav.py /
-- test_trainer_scene_nav.py's negative-control shape.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

h.tap(h.KEY.DOWN,    30)
h.tap(h.KEY.DOWN,    60)
h.tap(h.KEY.DOWN,    90)
h.tap(h.KEY.A,      130)

h.tap(h.KEY.SELECT, 170)
h.tap(h.KEY.SELECT, 210)
h.tap(h.KEY.SELECT, 250)

h.tap(h.KEY.B,      290)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("box_nav: OK")
end)
