-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 nulvox
--
-- Save-blocks-input drive: verify that selecting Save & Exit renders the
-- blocking "SAVING... / DON'T TURN OFF THE POWER!" splash screen and ignores
-- D-pad input while the save/flush is in flight.
--
-- Phases:
--   1. PRE_SAVE (frame 80)  — main_menu at boot with cursor on row 0
--      ("> Trainer").  Dumped to /dump/fb_pre.ppm.
--   2. Walk cursor DOWN×4 (frames 100, 130, 160, 190) to Save & Exit (row 4)
--      and tap A at frame 220 to enter save_scene.
--   3. SAVING (frames 221..~240) — while save_scene is displaying the splash
--      and flushing to FLASH:
--        - tap DOWN at frame 221 (held 221..224, spanning save_scene's
--          phase 0 display frame and phase 1 FLASH write) and again at 226
--        - snapshot /dump/fb_saving.ppm at frame 224 while the splash is
--          on screen.
--   4. POST_SAVE (frame 310) — after save_scene finishes and transitions
--      back to main_menu (which constructs with cursor_ = 0).  Dumped to
--      /dump/fb_post.ppm.  Must match PRE_SAVE byte-for-byte, proving the
--      in-flight DOWN inputs did not move the cursor off row 0.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

-- Phase 1: baseline main_menu with cursor on "> Trainer" (row 0)
h.at(80, function() h.fb_dump("/dump/fb_pre.ppm") end)

-- Walk Trainer(0) → Party(1) → Bag(2) → Boxes(3) → Save & Exit(4)
h.tap(h.KEY.DOWN, 100)
h.tap(h.KEY.DOWN, 130)
h.tap(h.KEY.DOWN, 160)
h.tap(h.KEY.DOWN, 190)

-- Enter save_scene
h.tap(h.KEY.A,    220, 1)

-- Attempt D-pad navigation while save_scene is active (both on its initial
-- display frame and during the synchronous FLASH write) and capture the
-- mid-save splash framebuffer.
h.tap(h.KEY.DOWN, 222, 3)
h.at(224, function() h.fb_dump("/dump/fb_saving.ppm") end)
h.tap(h.KEY.DOWN, 226, 3)

-- Phase 4: post-save main_menu snapshot
h.at(310, function() h.fb_dump("/dump/fb_post.ppm") end)

callbacks:add("shutdown", function()
    console:log("save_blocks_input: OK")
end)
