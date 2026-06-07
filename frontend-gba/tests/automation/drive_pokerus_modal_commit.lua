-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- CP#3 Wave 2B — Pokerus modal: open, cycle status, commit, save & exit.
--
-- The rs.sav fixture's slot 0 has Pokerus byte 0x4A (strain=4, days=10,
-- status=ACTIVE) by accident — the fixture generator packs IVs into the
-- same byte the gen3 pokerus reader uses.  We don't care about the
-- baseline value; we drive an explicit transition and assert.
--
-- Sequence:
--   1. Boot → main_menu (Trainer=0).
--   2. DOWN (30) → Party (1).
--   3. A    (60) → party_scene.
--   4. A    (100) → pokemon_edit_scene slot 0.  Cursor on ROW_SPECIES.
--   5. DOWN×7 (140..380) → walk to ROW_POKERUS.
--   6. A    (420) → open Pokerus modal.
--   7. LEFT (470) → cycle status ACTIVE → NEVER (per cycle_status -1 wrap:
--                   NEVER(0) -1 mod 3 = 2(CURED), CURED -1 = ACTIVE,
--                   ACTIVE -1 = NEVER; from baseline ACTIVE, LEFT once
--                   yields NEVER).
--   8. A    (520) → commit; writes strain=0, days=0, returns to editor.
--   9. B    (570) → editor → party_scene.
--  10. B    (610) → party_scene → main_menu.
--  11. DOWN×4 (650/690/730/770) → walk to Save & Exit.
--  12. A    (810) → save_scene → flush_to_flash.
--  13. shutdown → dump FLASH.
--
-- Post-conditions: slot 0 pokerus status NEVER, strain=0, days=0.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

h.tap(h.KEY.DOWN, 30)
h.tap(h.KEY.A,    60)
h.tap(h.KEY.A,    100)

h.tap(h.KEY.DOWN, 140)
h.tap(h.KEY.DOWN, 180)
h.tap(h.KEY.DOWN, 220)
h.tap(h.KEY.DOWN, 260)
h.tap(h.KEY.DOWN, 300)
h.tap(h.KEY.DOWN, 340)
h.tap(h.KEY.DOWN, 380)

-- Open modal
h.tap(h.KEY.A,    420)
-- Cycle status to NEVER
h.tap(h.KEY.LEFT, 470)
-- Commit
h.tap(h.KEY.A,    520)
-- Back to party, back to main menu
h.tap(h.KEY.B,    570)
h.tap(h.KEY.B,    610)
-- Walk to Save & Exit (Trainer=0 → Party → Bag → Boxes → Save)
h.tap(h.KEY.DOWN, 650)
h.tap(h.KEY.DOWN, 690)
h.tap(h.KEY.DOWN, 730)
h.tap(h.KEY.DOWN, 770)
h.tap(h.KEY.A,    810)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("pokerus_modal_commit: OK")
end)
