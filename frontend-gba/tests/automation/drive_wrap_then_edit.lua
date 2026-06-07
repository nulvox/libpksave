-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- Drive script proving main_menu cursor WRAPS from index 4 (Save & Exit)
-- back to index 0 (Trainer) when DOWN is pressed past the end.
--
-- main_menu_scene.cpp uses `cursor_ = (cursor_ + 1) % ENTRY_COUNT` on every
-- DOWN press, so 5 DOWN taps starting from cursor=0 should walk
-- 0 → 1 → 2 → 3 → 4 → 0 (wrap). After the 5th DOWN, the cursor must be on
-- Trainer (index 0) — proven by A successfully opening trainer_scene and
-- the subsequent text_input edit committing.
--
-- Sequence:
--   1. 5× DOWN (frames 30,60,90,120,150) → if wrap works, cursor lands at 0.
--   2. A → trainer_scene (cursor on ROW_NAME=0).
--   3. A → text_input_scene (initial buffer "RED", grid cursor at 'A').
--   4. A → buffer becomes "REDA" (4 of 7 max).
--   5. START → text_input commits, return to trainer; push_to_save dirties.
--   6. B → back to main_menu (cursor reset to 0 = Trainer).
--   7. DOWN ×4 → cursor walks to Save & Exit (index 4).
--   8. A on Save & Exit → save_scene → bridge_.flush_to_flash() writes FLASH.
--   9. Shutdown callback dumps the full FLASH region.
--
-- Failure modes distinguished by trainer.name in the FLASH dump:
--   "REDA"  → wrap worked + Trainer reached + text_input committed + saved.
--   "RED"   → wrap broke (cursor stuck at 4); 6th tap A immediately saved,
--             so no edit happened.
--
-- Inter-tap gaps of ~30–40 frames give butano's edge-triggered presses
-- and the scene swap time to settle (matches the gate-J cadence).

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

-- 5× DOWN — wraps cursor 0→1→2→3→4→0.
h.tap(h.KEY.DOWN, 30)
h.tap(h.KEY.DOWN, 60)
h.tap(h.KEY.DOWN, 90)
h.tap(h.KEY.DOWN, 120)
h.tap(h.KEY.DOWN, 150)

-- A → trainer_scene (only succeeds if wrap left cursor at index 0).
h.tap(h.KEY.A, 185)

-- A on ROW_NAME → text_input_scene.
h.tap(h.KEY.A, 220)

-- A → append 'A' (UPPER tab, grid pos 0) → buffer becomes "REDA".
h.tap(h.KEY.A, 260)

-- START → commit text_input, return to trainer_scene.
h.tap(h.KEY.START, 295)

-- B → back to main_menu (cursor reset to 0).
h.tap(h.KEY.B, 335)

-- DOWN ×4 → Trainer=0 → Party=1 → Bag=2 → Boxes=3 → Save & Exit=4.
h.tap(h.KEY.DOWN, 370)
h.tap(h.KEY.DOWN, 400)
h.tap(h.KEY.DOWN, 430)
h.tap(h.KEY.DOWN, 460)

-- A on Save & Exit → flush_to_flash.
h.tap(h.KEY.A, 495)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("wrap: OK")
end)
