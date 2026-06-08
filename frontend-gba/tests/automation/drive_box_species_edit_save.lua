-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- CP#5 tracer-bullet — box-slot species round-trip through the active-edit
-- gate, mirrored on drive_party_species_edit_save.lua.
--
-- The fixture (libpksave/tests/fixtures/generate_fixtures.py
-- gen3_make_save) seeds box 1 slot 0 with Bulbasaur at level 5; box 0 is
-- empty.  We edit box 1 slot 0 so an off-by-one in the box-index handoff
-- (writing to box 0 instead of 1) shows up immediately.
--
-- Sequence:
--   1. Boot → main_menu (cursor on Trainer=0).
--   2. DOWN ×3 (30/60/90) → cursor walks to Boxes (3).
--   3. A      (130) → box_scene at box 0, slot 0.
--   4. SELECT (170) → cycle to box 1 (where the fixture mon lives).
--   5. A      (210) → pokemon_edit_scene from box 1 slot 0 (NAV,
--                     IDENTITY, cursor on ROW_SPECIES).  "edit: OK" logs.
--   6. A      (250) → enter ACTIVE on ROW_SPECIES.
--   7. RIGHT  (290) → cycler steps species +1 (Bulbasaur → Ivysaur).
--                     Push to save via pksave_set_box_slot; mark dirty.
--   8. A      (330) → commit (exit ACTIVE → NAV).
--   9. B      (370) → pokemon_edit_scene → box_scene.
--  10. B      (410) → box_scene → main_menu (cursor resets to Trainer=0).
--  11. DOWN ×4 (440/470/500/530) → cursor walks to Save & Exit (4).
--  12. A      (560) → save_scene → bridge_.flush_to_flash().
--  13. shutdown → dump FLASH so the test can decode & assert.
--
-- Frame budget mirrors test_party_species_roundtrip.py (600 frames).

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

h.tap(h.KEY.DOWN,   30)
h.tap(h.KEY.DOWN,   60)
h.tap(h.KEY.DOWN,   90)
h.tap(h.KEY.A,     130)

h.tap(h.KEY.SELECT, 170)
h.tap(h.KEY.A,     210)

-- Active-edit gate: A enters ACTIVE on ROW_SPECIES.
h.tap(h.KEY.A,     250)
-- Cycler RIGHT: species +1 (1 → 2 = Bulbasaur → Ivysaur).
h.tap(h.KEY.RIGHT, 290)
-- Commit: A exits ACTIVE → NAV.
h.tap(h.KEY.A,     330)

h.tap(h.KEY.B,     370)
h.tap(h.KEY.B,     410)

h.tap(h.KEY.DOWN,  440)
h.tap(h.KEY.DOWN,  470)
h.tap(h.KEY.DOWN,  500)
h.tap(h.KEY.DOWN,  530)

h.tap(h.KEY.A,     560)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("box_edit: OK")
end)
