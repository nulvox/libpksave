-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 nulvox
--
-- Box-source Pokerus modal: open from IDENTITY on box 1 slot 0, B cancels,
-- return to pokemon_edit_scene (still in BOX source mode so B returns to
-- box_scene), no FLASH writes.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

-- Main menu: Trainer(0) → Party(1) → Bag(2) → Boxes(3)
h.tap(h.KEY.DOWN,   30)
h.tap(h.KEY.DOWN,   60)
h.tap(h.KEY.DOWN,   90)
h.tap(h.KEY.A,     130)

-- Box scene: SELECT → box 1, A → slot 0
h.tap(h.KEY.SELECT, 170)
h.tap(h.KEY.A,     210)

-- Walk cursor: SPECIES → NICKNAME → OT_NAME → ITEM → ABILITY → NATURE → SHINY → POKERUS
h.tap(h.KEY.DOWN,  250)
h.tap(h.KEY.DOWN,  290)
h.tap(h.KEY.DOWN,  330)
h.tap(h.KEY.DOWN,  370)
h.tap(h.KEY.DOWN,  410)
h.tap(h.KEY.DOWN,  450)
h.tap(h.KEY.DOWN,  490)

-- A: open Pokerus modal for box 1 slot 0
h.tap(h.KEY.A,     530)

-- B: cancel modal → return to pokemon_edit_scene
h.tap(h.KEY.B,     580)

-- B → box_scene
h.tap(h.KEY.B,     630)
-- B → main_menu
h.tap(h.KEY.B,     670)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("box_pokerus_modal_cancel: OK")
end)
