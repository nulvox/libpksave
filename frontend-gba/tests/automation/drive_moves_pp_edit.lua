-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- CP#3 Wave 2C — MOVES tab PP edit round-trip.
--
-- Drives to MOVES tab, navigates to ROW_PP_0 (row 1), enters ACTIVE,
-- decrements PP by 10 (LEFT tap on numeric_field).  Fixture seeds
-- move_pp[0] = 35; expect 25 after commit.  Tackle (id 33) base PP
-- 35 → cap 56, so 25 is well within range and no clamp occurs.
--
-- Sequence:
--   1. Boot → main_menu.
--   2. DOWN (30)   → Party.
--   3. A    (60)   → party_scene.
--   4. A    (100)  → editor IDENTITY/NAV.
--   5. LEFT (140)  → tab IDENTITY → MOVES (wrap).
--   6. DOWN (180)  → ROW_MOVE_0 → ROW_PP_0.
--   7. A    (220)  → enter ACTIVE on ROW_PP_0.
--   8. LEFT (260)  → PP 35 → 25.
--   9. A    (300)  → commit.
--  10. B    (340)  → editor → party.
--  11. B    (380)  → party → main_menu.
--  12. DOWN×4 (420/460/500/540) → walk to Save & Exit.
--  13. A    (580)  → save_scene → flush.
--  14. shutdown    → dump.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

h.tap(h.KEY.DOWN, 30)
h.tap(h.KEY.A,    60)
h.tap(h.KEY.A,    100)

h.tap(h.KEY.LEFT, 140)    -- IDENTITY → MOVES (wrap)
h.tap(h.KEY.DOWN, 180)    -- ROW_MOVE_0 → ROW_PP_0
h.tap(h.KEY.A,    220)    -- enter ACTIVE
h.tap(h.KEY.LEFT, 260)    -- PP -10  (35 → 25)
h.tap(h.KEY.A,    300)    -- commit

h.tap(h.KEY.B, 340)
h.tap(h.KEY.B, 380)

h.tap(h.KEY.DOWN, 420)
h.tap(h.KEY.DOWN, 460)
h.tap(h.KEY.DOWN, 500)
h.tap(h.KEY.DOWN, 540)

h.tap(h.KEY.A, 580)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("moves_pp_edit: OK")
end)
