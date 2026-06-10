-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 gbsdk contributors
--
-- Dirty-indicator drive: snapshot main_menu's header at three phases of
-- the dirty/clean cycle, dumping a PPM each time.
--
-- Phases:
--   1. CLEAN  — fresh boot, main_menu shown for the first time. Bridge
--      dirty=false (see pksave_bridge.cpp ctor). Header reads
--      "POKEMON RUBY" without the trailing "*". → /dump/fb_clean.ppm
--   2. DIRTY  — entered Trainer scene, bumped TID via numeric_field
--      (RIGHT taps), pressed B back to main_menu. push_to_save() inside
--      trainer_scene.cpp marks the bridge dirty; main_menu_scene
--      observes the flag flip and redraws the header with " *" suffixed.
--      → /dump/fb_dirty.ppm
--   3. AFTER_SAVE — selected Save & Exit, flush_to_flash() succeeded,
--      bridge cleared dirty. Header is back to "POKEMON RUBY".
--      → /dump/fb_after_save.ppm
--
-- This matches the dirty-indicator contract in
-- docs/frontend-gba-plan.md §"Build & save plumbing" #3.
--
-- Timing notes:
--   * Boot + FLASH detect + first main_menu draw takes ~60 frames in
--     practice; we snapshot at 80 to give bn::core::init and the lazy
--     FLASH unlock a generous buffer (see G1 retraction in
--     docs/frontend-gba-harness-plan.md — frame-0 probes are unsafe).
--   * The trainer-scene drive sequence below mirrors drive_tid_edit_save.lua
--     exactly through the B-back-to-main_menu step; only difference is
--     we then snapshot mid-sequence before continuing to Save & Exit.
--     Keeping the cadence identical means timing bugs found in the TID
--     test repro here, and vice versa.
--   * After A-on-Save & Exit the save_scene shows the "SAVING…" splash
--     for a few frames while the FLASH write completes. We give it ~80
--     frames of slack before the AFTER_SAVE snapshot — by then
--     save_scene::update() has called bridge.flush_to_flash() (which
--     clears dirty) and returned us to main_menu_scene, which has
--     re-rendered the header WITHOUT the " *".

local h = dofile("/src/frontend-gba/tests/automation/helpers.lua")

-- ── Phase 1: CLEAN ────────────────────────────────────────────────────────
h.at(80, function() h.fb_dump("/dump/fb_clean.ppm") end)

-- ── Drive into trainer_scene and dirty the bridge ────────────────────────
-- Mirrors drive_tid_edit_save.lua frames 100-200.
h.tap(h.KEY.A,     100)  -- main_menu Trainer → trainer_scene
h.tap(h.KEY.DOWN,  130)  -- ROW_NAME → ROW_TID (axis disarms on arrival)
h.tap(h.KEY.RIGHT, 160)  -- +10 on TID — push_to_save → mark_dirty
h.tap(h.KEY.B,     200)  -- back to main_menu_scene (still dirty)

-- ── Phase 2: DIRTY ────────────────────────────────────────────────────────
-- main_menu_scene::update detects the dirty-flag change and redraws the
-- header before the next vblank, so 30 frames after B is ample.
h.at(240, function() h.fb_dump("/dump/fb_dirty.ppm") end)

-- ── Drive Save & Exit to clear dirty ─────────────────────────────────────
-- main_menu cursor reset to Trainer (index 0) on every B-return.
-- Save & Exit = index 4, so DOWN×4.
h.tap(h.KEY.DOWN, 270)
h.tap(h.KEY.DOWN, 300)
h.tap(h.KEY.DOWN, 330)
h.tap(h.KEY.DOWN, 360)
h.tap(h.KEY.A,    400)   -- Save & Exit → save_scene → flush → main_menu

-- ── Phase 3: AFTER_SAVE ──────────────────────────────────────────────────
-- save_scene's UI-blocking splash + FLASH bank rewrite takes ~30-40
-- frames in practice; 80 frames of slack to be conservative.
h.at(480, function() h.fb_dump("/dump/fb_after_save.ppm") end)

callbacks:add("shutdown", function()
    console:log("dirty: OK")
end)
