-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- CP#3 Wave 2B — STATS tab level edit round-trip.
--
-- Drives to POKEMON_EDIT STATS tab and bumps level via RIGHT (×3 = +30
-- on the numeric_field, since RIGHT/LEFT are ±10 on numeric_field per
-- widget docblock).  Level starts at 5 → expect 35 post-save.
--
-- Frame budget: ~720 frames.  STATS tab walks need to advance to a
-- deeper row but for ROW_LEVEL we don't need any DOWN — it's row 0 of
-- the STATS list.
--
-- Sequence:
--   1. Boot → main_menu (Trainer=0).
--   2. DOWN (30)   → Party (1).
--   3. A    (60)   → party_scene.
--   4. A    (100)  → pokemon_edit_scene slot 0; tab=IDENTITY, NAV.
--   5. RIGHT (140) → tab IDENTITY → STATS.
--   6. A    (180)  → enter ACTIVE on ROW_LEVEL.
--   7. RIGHT (220) → level +10  (5 → 15)
--   8. RIGHT (260) → level +10  (15 → 25)
--   9. RIGHT (300) → level +10  (25 → 35)
--  10. A    (340)  → commit (exit ACTIVE → NAV).
--  11. B    (380)  → editor → party.
--  12. B    (420)  → party → main_menu.
--  13. DOWN×4 (460/500/540/580) → walk to Save & Exit.
--  14. A    (620)  → save_scene → flush_to_flash.
--  15. shutdown    → dump FLASH.
--
-- Expected post-condition: party[0].level == 35; species unchanged.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

h.tap(h.KEY.DOWN, 30)
h.tap(h.KEY.A,    60)
h.tap(h.KEY.A,    100)

-- IDENTITY → STATS
h.tap(h.KEY.RIGHT, 140)
-- Enter ACTIVE on ROW_LEVEL
h.tap(h.KEY.A,     180)
-- +30 in three RIGHT taps
h.tap(h.KEY.RIGHT, 220)
h.tap(h.KEY.RIGHT, 260)
h.tap(h.KEY.RIGHT, 300)
-- Commit
h.tap(h.KEY.A,     340)
-- Back out
h.tap(h.KEY.B, 380)
h.tap(h.KEY.B, 420)

h.tap(h.KEY.DOWN, 460)
h.tap(h.KEY.DOWN, 500)
h.tap(h.KEY.DOWN, 540)
h.tap(h.KEY.DOWN, 580)

h.tap(h.KEY.A, 620)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("stats_level_edit: OK")
end)
