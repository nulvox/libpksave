-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- CP#3 Wave 2A — party-slot species round-trip through the active-edit gate.
--
-- Wave 2A introduced a NAV/ACTIVE mode toggle inside pokemon_edit_scene:
-- D-pad LEFT/RIGHT in NAV mode cycles the IDENTITY/STATS/MOVES tabs, not
-- the focused field.  To cycle the species cycler the user must first
-- press A on ROW_SPECIES to enter ACTIVE mode, then RIGHT/LEFT, then A
-- (or B) to leave ACTIVE.
--
-- Sequence:
--   1. Boot → main_menu (cursor on Trainer=0).
--   2. DOWN (30)   → cursor to Party (1).
--   3. A    (60)   → party_scene; cursor on slot 0.
--   4. A    (100)  → pokemon_edit_scene for slot 0 (NAV, IDENTITY,
--                    cursor on ROW_SPECIES).  Scene logs "edit: OK"
--                    and "edit: tab=0".
--   5. A    (140)  → enter ACTIVE on ROW_SPECIES.
--   6. RIGHT (180) → cycler steps species +1.  Push to save, mark dirty.
--   7. A    (220)  → commit (exit ACTIVE → NAV).
--   8. B    (260)  → pokemon_edit_scene → party_scene.
--   9. B    (300)  → party_scene → main_menu (cursor resets to Trainer=0).
--  10. DOWN ×4 (330/360/390/420) → cursor walks to Save & Exit (index 4).
--  11. A    (450) → save_scene → bridge_.flush_to_flash().
--  12. shutdown   → dump FLASH so the test can decode & assert.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

h.tap(h.KEY.DOWN, 30)
h.tap(h.KEY.A,    60)
h.tap(h.KEY.A,    100)

-- Active-edit gate: A enters ACTIVE on ROW_SPECIES.
h.tap(h.KEY.A,     140)
-- Cycler RIGHT: species +1 (1 → 2 = Bulbasaur → Ivysaur).
h.tap(h.KEY.RIGHT, 180)
-- Commit: A exits ACTIVE → NAV.
h.tap(h.KEY.A,     220)

h.tap(h.KEY.B, 260)
h.tap(h.KEY.B, 300)

h.tap(h.KEY.DOWN, 330)
h.tap(h.KEY.DOWN, 360)
h.tap(h.KEY.DOWN, 390)
h.tap(h.KEY.DOWN, 420)

h.tap(h.KEY.A, 450)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("party_edit: OK")
end)
