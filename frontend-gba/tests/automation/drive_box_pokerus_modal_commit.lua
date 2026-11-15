-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 nulvox
--
-- Box-source Pokerus modal commit: open from box 1 slot 0, cycle status
-- ACTIVE → NEVER, commit (A), back out through box_scene to main_menu,
-- Save & Exit.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

-- Main menu: Trainer(0) → Party(1) → Bag(2) → Boxes(3)
h.tap(h.KEY.DOWN,   30)
h.tap(h.KEY.DOWN,   60)
h.tap(h.KEY.DOWN,   90)
h.tap(h.KEY.A,     130)

-- Box scene: SELECT → box 1, A → slot 0
h.tap(h.KEY.SELECT, 170)
h.tap(h.KEY.A,     210)

-- Walk cursor to ROW_POKERUS (row 7)
h.tap(h.KEY.DOWN,  250)
h.tap(h.KEY.DOWN,  290)
h.tap(h.KEY.DOWN,  330)
h.tap(h.KEY.DOWN,  370)
h.tap(h.KEY.DOWN,  410)
h.tap(h.KEY.DOWN,  450)
h.tap(h.KEY.DOWN,  490)

-- Open modal
h.tap(h.KEY.A,     530)
-- Cycle status ACTIVE → NEVER
h.tap(h.KEY.LEFT,  580)
-- Commit
h.tap(h.KEY.A,     630)
-- Back to box_scene, back to main_menu
h.tap(h.KEY.B,     680)
h.tap(h.KEY.B,     720)

-- Walk to Save & Exit (Trainer=0 → Party → Bag → Boxes → Save)
h.tap(h.KEY.DOWN,  760)
h.tap(h.KEY.DOWN,  800)
h.tap(h.KEY.DOWN,  840)
h.tap(h.KEY.DOWN,  880)
h.tap(h.KEY.A,     920)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("box_pokerus_modal_commit: OK")
end)
