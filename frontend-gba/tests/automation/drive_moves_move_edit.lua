-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- CP#3 Wave 2C — MOVES tab move-id round-trip.
--
-- Drives to POKEMON_EDIT MOVES tab, enters ACTIVE on ROW_MOVE_0,
-- bumps move id from 33 (Tackle) to 34 (Body Slam) via one RIGHT tap,
-- commits, save & exit.  Slot 0 of the rs.sav fixture is seeded with
-- move 33 / PP 35 per libpksave/tests/fixtures/generate_fixtures.py.
--
-- Body Slam (id 34) base PP = 15 → cap = floor(15 * 1.6) = 24.  The
-- old PP (35) exceeds the new cap and must be clamped down to 24.
--
-- Sequence:
--   1. Boot → main_menu (Trainer=0).
--   2. DOWN (30)   → Party.
--   3. A    (60)   → party_scene.
--   4. A    (100)  → editor IDENTITY/NAV/ROW_SPECIES.
--   5. LEFT (140)  → tab IDENTITY → MOVES (wrap).
--   6. A    (180)  → enter ACTIVE on ROW_MOVE_0.
--   7. RIGHT (220) → cycle move id 33 → 34.
--   8. A    (260)  → commit (NAV; PP cap reseated & clamped to 24).
--   9. B    (300)  → editor → party.
--  10. B    (340)  → party → main_menu.
--  11. DOWN×4 (380/420/460/500) → walk to Save & Exit.
--  12. A    (540)  → save_scene → flush_to_flash.
--  13. shutdown    → dump FLASH.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

h.tap(h.KEY.DOWN, 30)
h.tap(h.KEY.A,    60)
h.tap(h.KEY.A,    100)

h.tap(h.KEY.LEFT,  140)   -- IDENTITY → MOVES (wrap)
h.tap(h.KEY.A,     180)   -- enter ACTIVE on ROW_MOVE_0
h.tap(h.KEY.RIGHT, 220)   -- move 33 → 34
h.tap(h.KEY.A,     260)   -- commit

h.tap(h.KEY.B, 300)
h.tap(h.KEY.B, 340)

h.tap(h.KEY.DOWN, 380)
h.tap(h.KEY.DOWN, 420)
h.tap(h.KEY.DOWN, 460)
h.tap(h.KEY.DOWN, 500)

h.tap(h.KEY.A, 540)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("moves_move_edit: OK")
end)
