-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- Gate J — full trainer-name round-trip drive script.
--
-- Sequence (verified against frontend-gba/src/scenes/*.cpp):
--   1. Boot finishes ~frame 15, main menu draws with cursor on Trainer.
--   2. A on Trainer entry → trainer_scene (cursor on ROW_NAME).
--   3. A on Name row → text_input_scene (initial buffer = current name
--      "RED", grid cursor at index 0 = 'A' on UPPER tab).
--   4. A three times → appends "AAA" → buffer "REDAAA" (6 of 7 max).
--   5. START → text_input commit, return to trainer_scene; consume_text_input_result
--      copies the new name into trainer_.name and push_to_save marks dirty.
--   6. B → return to main_menu (cursor reset to 0 = Trainer).
--   7. DOWN ×4 → cursor walks to Save & Exit (index 4).
--   8. A on Save & Exit → save_scene → bridge_.flush_to_flash() writes FLASH.
--   9. shutdown callback dumps the full FLASH region.
--
-- All `tap()` calls hold for 3 frames (helpers default). Inter-action gaps
-- of ~30 frames give the scene loop time to process the edge-triggered
-- a_pressed/b_pressed/start_pressed events and complete the scene swap.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

-- main_menu → trainer
h.tap(h.KEY.A, 30)

-- trainer → text_input (name field)
h.tap(h.KEY.A, 60)

-- Type "AAA" extending "RED" → "REDAAA"
h.tap(h.KEY.A, 100)
h.tap(h.KEY.A, 130)
h.tap(h.KEY.A, 160)

-- Commit text_input
h.tap(h.KEY.START, 190)

-- Back to main_menu
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
    console:log("j: OK")
end)
