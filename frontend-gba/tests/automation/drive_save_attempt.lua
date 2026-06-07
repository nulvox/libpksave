-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- Drive the exact Save & Exit input sequence from gate J
-- (drive_name_edit_save.lua) onto an editor whose sidecar save is a
-- 128 KiB blob of 0xAB.  pksave_detect must return UNKNOWN on this
-- payload, so make_initial_scene lands on boot_error_scene — a halt
-- scene that ignores all input and never transitions.
--
-- Sequence (mirroring gate J's terminal navigation):
--   frame 30:  wait long enough for boot_error_scene to draw.
--   frame 60..150 (DOWN x4 @ 30-frame stride): would walk the main
--                menu cursor from Trainer → Save & Exit IF the editor
--                had fallen through to main_menu.
--   frame 180: A — would trigger save_scene → flush_to_flash IF on
--                main_menu's Save & Exit row.
--
-- On boot_error none of these taps do anything, so the FLASH mirror
-- mGBA holds in memory stays at the loaded fixture's 0xAB bytes for
-- the entire run.  The shutdown callback dumps the full FLASH region
-- so the Python harness can verify zero writes occurred.
--
-- Frame budget: 200 frames (per test_boot_rejects_bad_save.py) gives
-- ample headroom past frame 180.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

-- DOWN x4 to nominally walk Trainer(0) → Party(1) → Bag(2) → Boxes(3) → Save(4)
h.tap(h.KEY.DOWN, 60)
h.tap(h.KEY.DOWN, 90)
h.tap(h.KEY.DOWN, 120)
h.tap(h.KEY.DOWN, 150)

-- A to nominally trigger Save & Exit
h.tap(h.KEY.A, 180)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("boot_reject: OK")
end)
