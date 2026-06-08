-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- CP#4 round-trip: edit Items pocket slot 0 quantity through bag_scene
-- + slot_edit_scene + Save & Exit, then dump FLASH.
--
-- Fixture (rs.sav per libpksave/tests/fixtures/generate_fixtures.py):
--   Items slot 0 = Potion (id=13) ×5
--
-- list_view consumes UP/DOWN edges (see docs/frontend-gba-plan.md §Pitfall
-- "list_view consumes UP/DOWN edges"), so on a numeric_field row only
-- LEFT (-10) and RIGHT (+10) are reachable as edit keys — same constraint
-- as test_tid_roundtrip.py.  We use one RIGHT tap to bump qty 5 → 15.
--
-- Sequence:
--   30 DOWN  → cursor Trainer (0) → Party (1).
--   60 DOWN  → Party (1) → Bag (2).
--   90  A    → main_menu → bag_scene (ITEMS pocket).  Logs "bag: OK".
--  140  A    → slot_edit_scene for (Items, slot 0).  Logs "slot_edit: OK".
--              Initial state: ROW_ITEM focused (list_view sel=0), item
--              cycler seeded at 13 (Potion), qty seeded at 5.
--  180 DOWN  → list_view selects ROW_QTY (sel=1).  qty_.on_focus_acquired
--              disarms any axis held at arrival.
--  220 RIGHT → numeric_field RIGHT = +10 (qty 5 → 15).
--  260  A    → commit: pksave_set_item({13, 15}); mark dirty; return BAG.
--  300  B    → bag_scene → main_menu.
--  340-430   → DOWN ×4 to walk cursor down to Save & Exit (index 4).
--  460  A    → save_scene → bridge_.flush_to_flash().
--  shutdown  → dump FLASH so the test can decode & assert qty == 15.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

-- main_menu: walk to Bag (index 2).
h.tap(h.KEY.DOWN, 30)
h.tap(h.KEY.DOWN, 60)
h.tap(h.KEY.A,    90)

-- bag_scene → slot_edit_scene for slot 0.
h.tap(h.KEY.A,   140)

-- list_view: DOWN moves selection from ROW_ITEM (0) to ROW_QTY (1).
h.tap(h.KEY.DOWN, 180)

-- numeric_field on ROW_QTY: RIGHT = +10 (qty 5 → 15).  UP/DOWN are
-- list_view's nav axes; only LEFT/RIGHT survive on a numeric_field row
-- inside a list_view scene.
h.tap(h.KEY.RIGHT, 220)

-- Commit + back to bag, then back to main_menu.
h.tap(h.KEY.A,    260)
h.tap(h.KEY.B,    300)

-- main_menu: walk down to Save & Exit (index 4).  From Bag (2) we need
-- 2 more DOWN.  Plus one extra in case cursor reset on re-entry.
h.tap(h.KEY.DOWN, 340)
h.tap(h.KEY.DOWN, 370)
h.tap(h.KEY.DOWN, 400)
h.tap(h.KEY.DOWN, 430)
h.tap(h.KEY.A,    460)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("bag_edit: OK")
end)
