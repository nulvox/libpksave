-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- TID round-trip drive script — exercises the numeric_field widget.
--
-- numeric_field key mapping (from frontend-gba/src/widgets/numeric_field.cpp):
--     UP    held → +1     (line 64:  if (up_frames_    && should_step(...)) delta += 1)
--     DOWN  held → -1     (line 65)
--     RIGHT held → +10    (line 66)
--     LEFT  held → -10    (line 67)
--
-- Repeat behaviour (lines 11-19, 42-53):
--   * Frame 1 of a hold is always a step (`if (hold_frames == 1) return true`).
--   * Frames 2 through INITIAL_HOLD_DELAY-1 (=9) are dead zone — no step.
--   * From frame 10 onward, repeats fire every SLOW_INTERVAL (10) frames,
--     then MEDIUM (4) at >=40 held, then FAST (2) at >=80 held.
--
-- UX interaction with list_view: trainer_scene calls list_.tick() *before*
-- tid_.tick(), and list_view consumes UP/DOWN edges. So pressing UP/DOWN
-- on ROW_TID actually navigates off the row. L/R are NOT consumed by
-- list_view, so RIGHT (+10) and LEFT (-10) are the only editing keys.
--
-- (Older copies of this script worked around a now-fixed bug: navigating
-- DOWN onto ROW_TID also fired tid_.tick() on the same frame and debited
-- TID by 1, because numeric_field saw the still-held DOWN as a fresh
-- frame-1 step. trainer_scene now calls tid_.on_focus_acquired() on
-- selection change, which disarms axes held at arrival until they're
-- released. So the DOWN-to-arrive transition is now a pure no-op for the
-- numeric_field, and the editing sequence below is the only source of TID
-- delta.)
--
-- Sequence:
--   1. Boot, main_menu cursor on Trainer.
--   2. A → trainer_scene (cursor on ROW_NAME=0).
--   3. DOWN → ROW_TID=1. No TID change (axis disarmed on arrival).
--   4. RIGHT x3, each a 3-frame tap separated by ~30 frames → tid += 30.
--      Each tick calls push_to_save() which marks the bridge dirty.
--   5. B → main_menu (cursor reset).
--   6. DOWN x4 → Save & Exit (index 4).
--   7. A → save_scene → flush_to_flash().
--   8. shutdown callback dumps FLASH.
--
-- Net delta: +30. EXPECTED_TID = INITIAL_TID + 30.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

-- main_menu → trainer
h.tap(h.KEY.A, 30)

-- ROW_NAME → ROW_TID (this also fires tid_.tick() with down_held, so tid -= 1)
h.tap(h.KEY.DOWN, 60)

-- Three RIGHT taps on the TID numeric_field — each +10, total +30.
-- list_view ignores L/R so cursor stays on ROW_TID.
-- 30-frame gap between taps: ample time for right_frames_ to reset to 0
-- between presses, keeping each tap a clean single +10 edge.
h.tap(h.KEY.RIGHT, 100)
h.tap(h.KEY.RIGHT, 130)
h.tap(h.KEY.RIGHT, 160)

-- Back to main_menu
h.tap(h.KEY.B, 200)

-- Navigate to Save & Exit (Trainer=0 → Party=1 → Bag=2 → Boxes=3 → Save=4)
h.tap(h.KEY.DOWN, 230)
h.tap(h.KEY.DOWN, 260)
h.tap(h.KEY.DOWN, 290)
h.tap(h.KEY.DOWN, 320)

-- Trigger save
h.tap(h.KEY.A, 360)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("tid: OK")
end)
