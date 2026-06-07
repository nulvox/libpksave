-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- CP#3 Wave 2A — pokemon_edit_scene tab navigation.
--
-- Drives the editor into pokemon_edit_scene, then cycles tabs with
-- D-pad LEFT/RIGHT while in NAV mode (no field active-edit).  Each
-- tab change should print "edit: tab=<n>" via BN_LOG.  The drive
-- never enters ACTIVE mode (no A press on a row), so the species
-- value must remain at the fixture's initial value (Bulbasaur = 1)
-- after Save & Exit.
--
-- Sequence:
--   1. Boot → main_menu (cursor on Trainer = 0).
--   2. DOWN (30)   → cursor → Party (1).
--   3. A    (60)   → party_scene.
--   4. A    (100)  → pokemon_edit_scene (slot 0); ctor logs "edit: OK"
--                    and the initial tab as "edit: tab=0".
--   5. RIGHT (140) → tab cycles IDENTITY → STATS; expect "edit: tab=1".
--   6. RIGHT (180) → tab cycles STATS → MOVES; expect "edit: tab=2".
--   7. RIGHT (220) → tab wraps MOVES → IDENTITY; expect "edit: tab=0".
--   8. LEFT  (260) → tab cycles IDENTITY → MOVES (wrap); expect "edit: tab=2".
--   9. B    (300)  → return to party_scene.
--  10. B    (340)  → return to main_menu (cursor → Trainer = 0).
--  11. DOWN×4 (370/400/430/460) → cursor to Save & Exit.
--  12. A    (490) → save_scene → flush_to_flash.
--  13. shutdown   → dump FLASH.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

h.tap(h.KEY.DOWN, 30)
h.tap(h.KEY.A,    60)
h.tap(h.KEY.A,    100)

h.tap(h.KEY.RIGHT, 140)
h.tap(h.KEY.RIGHT, 180)
h.tap(h.KEY.RIGHT, 220)
h.tap(h.KEY.LEFT,  260)

h.tap(h.KEY.B, 300)
h.tap(h.KEY.B, 340)

h.tap(h.KEY.DOWN, 370)
h.tap(h.KEY.DOWN, 400)
h.tap(h.KEY.DOWN, 430)
h.tap(h.KEY.DOWN, 460)

h.tap(h.KEY.A, 490)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("tab_nav: OK")
end)
