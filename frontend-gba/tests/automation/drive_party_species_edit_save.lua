-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- CP#3-C tracer bullet — party-slot species round-trip.
--
-- Sequence (verified against frontend-gba/src/scenes/*.cpp):
--   1. Boot finishes ~frame 15, main_menu draws (cursor on Trainer=0).
--   2. DOWN tap (frame 30) → cursor moves to Party (index 1).
--   3. A tap (frame 60)    → party_scene constructs.  Cursor starts at slot 0.
--   4. A tap (frame 100)   → pokemon_edit_scene for slot 0.  Cursor starts
--                            on ROW_SPECIES.  Scene logs "edit: OK".
--   5. RIGHT tap (frame 140) → cycler steps species +1.  pokemon_edit_scene
--                              calls pksave_set_party_slot and marks dirty.
--   6. B tap (frame 180)   → return to party_scene.
--   7. B tap (frame 220)   → return to main_menu (cursor reset to Trainer=0).
--   8. DOWN ×4 (250/280/310/340) → cursor walks to Save & Exit (index 4).
--   9. A tap (frame 370)   → save_scene → bridge_.flush_to_flash().
--  10. shutdown callback dumps FLASH so the test can decode & assert.
--
-- Frame budget: 540 (boot + 10 input edges, plus FLASH write headroom).
-- The trainer-name round-trip uses 480 with a longer text-input subscene;
-- this drive has fewer text-input frames but a Save & Exit walk identical
-- in shape (DOWN ×4 from a freshly-reset main_menu cursor).

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

-- main_menu: DOWN moves cursor from Trainer (0) to Party (1)
h.tap(h.KEY.DOWN, 30)

-- main_menu → party_scene
h.tap(h.KEY.A, 60)

-- party_scene → pokemon_edit_scene (slot 0 is selected by default)
h.tap(h.KEY.A, 100)

-- Cycler RIGHT: species +1 (1 → 2 = Bulbasaur → Ivysaur in the fixture).
h.tap(h.KEY.RIGHT, 140)

-- pokemon_edit_scene → party_scene
h.tap(h.KEY.B, 180)

-- party_scene → main_menu (cursor resets to Trainer=0)
h.tap(h.KEY.B, 220)

-- Navigate to Save & Exit (Trainer=0 → Party=1 → Bag=2 → Boxes=3 → Save=4)
h.tap(h.KEY.DOWN, 250)
h.tap(h.KEY.DOWN, 280)
h.tap(h.KEY.DOWN, 310)
h.tap(h.KEY.DOWN, 340)

-- Trigger save
h.tap(h.KEY.A, 370)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("party_edit: OK")
end)
