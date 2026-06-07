-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- CP#3 Wave 2B — Pokerus modal: open from IDENTITY, B cancels, no FLASH writes.
--
-- IDENTITY tab row order (8 rows; ROW_POKERUS = 7):
--   0 ROW_SPECIES, 1 ROW_NICKNAME, 2 ROW_OT_NAME, 3 ROW_ITEM,
--   4 ROW_ABILITY, 5 ROW_NATURE,   6 ROW_SHINY,   7 ROW_POKERUS
--
-- Sequence:
--   1. Boot → main_menu (cursor Trainer=0).
--   2. DOWN (30) → Party (1).
--   3. A    (60) → party_scene.
--   4. A    (100) → pokemon_edit_scene slot 0.  Cursor on ROW_SPECIES.
--   5. DOWN×7 (140..) → walk cursor down to ROW_POKERUS.
--   6. A → open Pokerus modal.  Scene logs "pokerus: OK".
--   7. B → cancel; back to pokemon_edit_scene without committing.
--   8. B → back to party_scene.
--   9. B → back to main_menu.
-- 10. shutdown → dump FLASH.  No Save & Exit so the entire FLASH region
--     must remain 0xFF (assuming sidecar load didn't touch it post-boot).
--
-- Asserting we did NOT save and the modal exited cleanly is the gate.
-- We deliberately never press A to commit anything.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

h.tap(h.KEY.DOWN, 30)
h.tap(h.KEY.A,    60)
h.tap(h.KEY.A,    100)

-- Walk cursor: SPECIES → NICKNAME → OT_NAME → ITEM → ABILITY → NATURE → SHINY → POKERUS
h.tap(h.KEY.DOWN, 140)
h.tap(h.KEY.DOWN, 180)
h.tap(h.KEY.DOWN, 220)
h.tap(h.KEY.DOWN, 260)
h.tap(h.KEY.DOWN, 300)
h.tap(h.KEY.DOWN, 340)
h.tap(h.KEY.DOWN, 380)

-- A: open Pokerus modal
h.tap(h.KEY.A, 420)

-- B: cancel modal → return to pokemon_edit_scene
h.tap(h.KEY.B, 470)

-- B → party_scene
h.tap(h.KEY.B, 520)
-- B → main_menu
h.tap(h.KEY.B, 560)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("pokerus_modal_cancel: OK")
end)
