-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- CP#3 Wave 2A — party slot 0 nickname round-trip.
--
-- Drives the editor through:
--   main_menu → Party → slot 0 → pokemon_edit_scene → DOWN (ROW_NICKNAME)
--   → A (drill into text_input) → A (type 'A') → START (commit)
--   → return to editor → B → party → B → main_menu → Save & Exit.
--
-- Fixture's slot 0 nickname is "BULBASAUR" (9 chars).  text_input's
-- initial grid cursor sits at 'A'; one A-tap appends 'A' → buffer
-- "BULBASAURA" (10 chars, the gen-3 nickname cap).  START commits.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

-- main_menu → party → editor
h.tap(h.KEY.DOWN, 30)
h.tap(h.KEY.A,    60)
h.tap(h.KEY.A,    100)

-- DOWN: ROW_SPECIES → ROW_NICKNAME
h.tap(h.KEY.DOWN, 140)
-- A: drill into text_input (NICKNAME target)
h.tap(h.KEY.A,    180)
-- A on the keyboard: place focused char ('A') into the buffer
h.tap(h.KEY.A,    220)
-- START: commit the edit, return to pokemon_edit_scene
h.tap(h.KEY.START, 260)

-- Editor receives the result on re-entry; push_to_save runs.
-- Back out to main_menu.
h.tap(h.KEY.B, 310)
h.tap(h.KEY.B, 350)

-- Walk to Save & Exit (Trainer=0 → Party=1 → Bag=2 → Boxes=3 → Save=4)
h.tap(h.KEY.DOWN, 380)
h.tap(h.KEY.DOWN, 410)
h.tap(h.KEY.DOWN, 440)
h.tap(h.KEY.DOWN, 470)

h.tap(h.KEY.A, 500)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("nickname_edit: OK")
end)
