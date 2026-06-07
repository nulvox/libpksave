-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- CP#3 Wave 2B — STATS tab EV edit round-trip.
--
-- Drives to POKEMON_EDIT STATS tab, navigates to ROW_EV_HP (row 1),
-- bumps EV by +30 via three RIGHT taps, commits, save, exit.
--
-- Fixture seeds EVs to 0; expected post-save HP EV = 30.
--
-- Sequence:
--   1. Boot → main_menu (Trainer=0).
--   2. DOWN (30) → Party.
--   3. A    (60) → party.
--   4. A    (100) → editor IDENTITY/NAV/ROW_SPECIES.
--   5. RIGHT (140) → tab IDENTITY → STATS, cursor on ROW_LEVEL (row 0).
--   6. DOWN (180)  → cursor ROW_LEVEL → ROW_EV_HP (row 1).
--   7. A    (220)  → enter ACTIVE on ROW_EV_HP.
--   8. RIGHT (260) → EV_HP +10  (0 → 10)
--   9. RIGHT (300) → EV_HP +10  (10 → 20)
--  10. RIGHT (340) → EV_HP +10  (20 → 30)
--  11. A    (380)  → commit (exit ACTIVE → NAV).
--  12. B    (420)  → editor → party.
--  13. B    (460)  → party → main_menu.
--  14. DOWN×4 (500/540/580/620) → walk to Save & Exit.
--  15. A    (660)  → save_scene → flush.
--  16. shutdown    → dump FLASH.

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

h.tap(h.KEY.DOWN, 30)
h.tap(h.KEY.A,    60)
h.tap(h.KEY.A,    100)

h.tap(h.KEY.RIGHT, 140)   -- IDENTITY → STATS
h.tap(h.KEY.DOWN,  180)   -- ROW_LEVEL → ROW_EV_HP
h.tap(h.KEY.A,     220)   -- enter ACTIVE
h.tap(h.KEY.RIGHT, 260)   -- +10
h.tap(h.KEY.RIGHT, 300)   -- +10
h.tap(h.KEY.RIGHT, 340)   -- +10
h.tap(h.KEY.A,     380)   -- commit

h.tap(h.KEY.B, 420)
h.tap(h.KEY.B, 460)

h.tap(h.KEY.DOWN, 500)
h.tap(h.KEY.DOWN, 540)
h.tap(h.KEY.DOWN, 580)
h.tap(h.KEY.DOWN, 620)

h.tap(h.KEY.A, 660)

callbacks:add("shutdown", function()
    h.sram_dump("/dump/sram.bin")
    console:log("stats_ev_edit: OK")
end)
