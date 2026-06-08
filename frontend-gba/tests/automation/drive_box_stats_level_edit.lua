-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- CP#5 + L11 — STATS-tab level edit round-trip on a box-resident mon.
--
-- Mirrors drive_stats_level_edit.lua except the entry point is Boxes,
-- not Party.  The library-side fix (L11) decodes box-mon level from
-- the Growth substruct's XP field and re-encodes XP from the new level
-- on write — without that, the level edit landed in the editor's UI
-- state but g3_encode_box_mon dropped it silently.
--
-- Fixture seeds box 1 slot 0 with Bulbasaur (MEDIUM_SLOW) at level 5;
-- target is 35 (= 5 + 3×10).
--
-- Sequence:
--   1. Boot → main_menu (Trainer=0).
--   2. DOWN×3 (30/60/90) → walk to Boxes (3).
--   3. A      (130)      → box_scene, box 0 / slot 0.
--   4. SELECT (170)      → box 0 → 1 (fixture's Bulbasaur).
--   5. A      (210)      → pokemon_edit_scene; tab=IDENTITY, NAV.
--   6. RIGHT  (250)      → tab IDENTITY → STATS.
--   7. A      (290)      → enter ACTIVE on ROW_LEVEL.
--   8. RIGHT  (330)      → level +10 (5 → 15).
--   9. RIGHT  (370)      → level +10 (15 → 25).
--  10. RIGHT  (410)      → level +10 (25 → 35).
--  11. A      (450)      → commit (exit ACTIVE → NAV).
--  12. B      (490)      → editor → box_scene.
--  13. B      (530)      → box_scene → main_menu.
--  14. DOWN×4 (570/610/650/690) → walk to Save & Exit.
--  15. A      (730)      → save_scene → flush_to_flash.
--  16. shutdown — dump FLASH.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

h.tap(h.KEY.DOWN,    30)
h.tap(h.KEY.DOWN,    60)
h.tap(h.KEY.DOWN,    90)
h.tap(h.KEY.A,      130)

h.tap(h.KEY.SELECT, 170)
h.tap(h.KEY.A,      210)

-- IDENTITY → STATS
h.tap(h.KEY.RIGHT,  250)
-- Enter ACTIVE on ROW_LEVEL
h.tap(h.KEY.A,      290)
-- +30 in three RIGHT taps
h.tap(h.KEY.RIGHT,  330)
h.tap(h.KEY.RIGHT,  370)
h.tap(h.KEY.RIGHT,  410)
-- Commit
h.tap(h.KEY.A,      450)

-- Back out
h.tap(h.KEY.B,      490)
h.tap(h.KEY.B,      530)

h.tap(h.KEY.DOWN,   570)
h.tap(h.KEY.DOWN,   610)
h.tap(h.KEY.DOWN,   650)
h.tap(h.KEY.DOWN,   690)

h.tap(h.KEY.A,      730)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("box_level_edit: OK")
end)
