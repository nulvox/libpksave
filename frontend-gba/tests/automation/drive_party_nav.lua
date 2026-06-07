-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- Navigation-only drive for the Party scene (CP#3-B).
--
-- Sequence (verified against frontend-gba/src/scenes/*.cpp):
--   1. Boot finishes ~frame 15, main menu draws with cursor on Trainer.
--      Requires a valid sidecar save: with no save, pksave_detect
--      returns UNKNOWN and main lands on boot_error_scene instead of
--      main_menu, so the drive never reaches party_scene.
--   2. DOWN tap (frame 30) → cursor moves to Party (index 1).
--   3. A tap (frame 60)    → party_scene constructs.  Its ctor calls
--      BN_LOG("party: OK"); the test greps stdout for that marker.
--   4. DOWN × 2 (100, 130) → walk a couple of slot rows.
--   5. A tap (frame 160)   → drill stub (stays on party_scene).
--   6. B tap (frame 200)   → return to main_menu.
--   7. shutdown callback dumps FLASH so the test can assert no writes.
--
-- Why no save step: navigation must not mutate FLASH.  The harness's
-- positive control is test_trainer_name_roundtrip.py; this is the
-- party-scene negative control (cf. test_trainer_scene_nav.py).

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

-- main_menu: DOWN moves cursor from Trainer (0) to Party (1)
h.tap(h.KEY.DOWN, 30)

-- main_menu → party_scene
h.tap(h.KEY.A, 60)

-- Walk inside party_scene
h.tap(h.KEY.DOWN, 100)
h.tap(h.KEY.DOWN, 130)

-- A on a slot: CP#3-B is a no-op drill (the slot index lands in
-- bridge::pending_party_slot for CP#3-C to consume).  The press must
-- not crash and must not flush FLASH; both are checked at shutdown.
h.tap(h.KEY.A, 160)

-- party_scene → main_menu
h.tap(h.KEY.B, 200)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("drive: OK")
end)
