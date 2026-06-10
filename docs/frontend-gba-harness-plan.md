<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 gbsdk contributors
-->

# `frontend-gba` mGBA-Lua harness — reference and extension guide

Originally drafted on 2026-06-06 as a parallel-work plan for the POC
and its hardening rounds. All planned tracks shipped; this doc now lives
as the **reference for the harness** — how the pieces fit together,
which patterns to follow when adding a new test, and which gotchas are
real. The CP#3 agent reads this *after* `docs/frontend-gba-plan.md`.

Companion docs:

- `docs/frontend-gba-plan.md` — master execution plan and the
  CP#3-onward roadmap. **Read this first.**
- `docs/frontend-gba-libpksave-gaps.md` — library-side gaps; the
  fixture-generator bug (Track B below) is also tracked there.

Date produced: 2026-06-06. Last refresh: 2026-06-07.

---

## What landed

The POC + five hardening rounds shipped **14 tests** plus a
numeric_field bug fix surfaced by the TID round-trip — that's the
inventory in the table below, frozen as the historical record of what
made the harness usable. Subsequent checkpoints (CP#3/4/5 scenes,
CP#6 sprite plumbing, the framebuffer dump for the dirty-indicator
assertion) added another 16 tests; the **current canonical
`make test-gba` inventory of 30 tests** lives in
`docs/frontend-gba-plan.md` §"Test inventory" and stays in sync with
the `GBA_TESTS` list in the root `Makefile`.

| POC + hardening test              | What it proves |
|-----------------------------------|----------------|
| `test_decode_fixture.py`          | ctypes decodes RS / Emerald / FRLG fixtures (no docker) |
| `test_mgba_harness_smoke.py`      | Python → docker → mgba-lua → stdout marker |
| `test_editor_rom_loads.py`        | pksave-gba.gba loads as a GBA cartridge |
| `test_sram_dump.py`               | 16-byte tracer through Lua → file → Python |
| `test_sram_dump_full.py`          | full SRAM/FLASH region via mirror-probe |
| `test_flash_detected.py`          | mGBA logs `Detected Flash savegame` after boot |
| `test_key_input.py`               | A-tap flips KEYINPUT bit 0 (`0x03ff → 0x03fe`) |
| `test_sidecar_save.py`            | `--save rs.sav` injects fixture's first 16 bytes into SRAM |
| `test_fixture_roundtrip.py`       | 3-variant boot → dump → decode (RS / Emerald / FRLG) |
| `test_trainer_scene_nav.py`       | navigation produces zero FLASH writes |
| `test_trainer_name_roundtrip.py`  | 3-variant trainer-name edit RED → REDAAA through FLASH |
| `test_boot_rejects_bad_save.py`   | malformed fixture → boot_error halt; FLASH untouched |
| `test_tid_roundtrip.py`           | TID +30 through numeric_field; name preserved |
| `test_main_menu_wrap.py`          | DOWN×5 wraps cursor back to Trainer |

Supporting infrastructure shipped alongside:

- `docker/test-gba/Dockerfile` + `mgba-lua.c` — custom headless harness;
  required because upstream mGBA's SDL frontend has **no** `--script`
  flag (Lua wiring is Qt-only). The custom binary embeds libmgba +
  the Lua engine directly. Pinned to `MGBA_TAG=0.10.5`.
- `libpksave/Makefile` — new `lib` / `libpksave.so` target compiled with
  all five `PKSAVE_GENx` flags for ctypes consumption.
- `frontend-gba/tests/automation/pksave_ctypes.py` — minimal Python
  wrapper exposing `detect`, `open`, `get_trainer_name`, `close`.
- `frontend-gba/tests/automation/framebuffer.py` — PPM parser + region
  hash / non-black-pixel helpers used by `test_dirty_indicator.py`.
  Pure stdlib (no Pillow). Paired with helpers.lua's `M.fb_dump(path)`
  wrapper around the custom `fb_dump_ppm` Lua global registered in
  `docker/test-gba/mgba-lua.c`. See "Framebuffer dump" gap note below.

Convention learned while probing: mGBA's Lua memory bindings
lazy-register named regions (`emu.memory.sram`) only after the first
absolute-address access via `emu:read8(0x0E000000)`. Lua scripts that
want the named accessor must do a warm-up byte read first.

---

## Gaps surfaced during the POC

### ~~G1~~ — **Retracted 2026-06-06.** Was a probe-script artifact, not a ROM bug

The POC observation `GBA Memory: Detected SRAM savegame` was an artifact
of `probe_sram_read.lua` doing a warm-up `emu:read8(0x0E000000)` **before**
the editor's boot code ran. mGBA's save-type autodetect is *lazy*: the
first cartridge-memory access decides — a write to the FLASH-unlock
address `0x0E005555` means FLASH, anything else means SRAM. The probe
script's read at `0x0E000000` triggered the SRAM path first.

The editor ROM has carried the canonical `FLASH1M_V103` magic string the
whole time — verified by `strings frontend-gba/pksave-gba.gba | grep FLASH1M`
and anchored against `--gc-sections` via
`frontend-gba/src/save_type_signature.cpp`. Once mgba-lua runs for
≥~15 frames so `bn::core::init` → `flash_read_all` → flash unlock fires,
mGBA logs `Detected Flash savegame` + `Updating flash chip from 512kb to 1Mb`.

The fix landed via Track A as a *test*, not a code change: ship a
regression test that asserts FLASH detection after 60 frames, so this
ordering subtlety can't quietly re-introduce a "detected SRAM" debugging
session. See `frontend-gba/tests/automation/test_flash_detected.py`.

**General lesson for future tests:** anything that probes mGBA-allocated
state (memory regions, save type, etc.) must run for enough frames that
the boot code has issued its initialising memory accesses. Don't assert
save-type or `emu.memory.<region>` membership at frame 0.

### G3 — `list_view` consumes UP/DOWN edges the same frame `numeric_field` reads them

**Surfaced 2026-06-07 by `test_tid_roundtrip.py`.**

`widgets/list_view.cpp` uses edge-triggered `up_pressed()/down_pressed()`
to move its cursor. `widgets/numeric_field.cpp` uses level-triggered
`up_held()/down_held()` with a frame counter (so a single tap = "frame
1" = one step). When `trainer_scene::update` runs both in the same
frame, navigating DOWN onto a numeric row both moves the cursor AND
debits the numeric value by 1, because the still-held DOWN edge is
observed by both readers.

**Fix landed 2026-06-07 in the same PR as the test that found it:**

- `widgets/numeric_field` now exposes `on_focus_acquired()` and tracks
  a per-axis `armed_` flag. Calling `on_focus_acquired()` disarms any
  currently-held axis; tick() ignores disarmed axes until they're
  released (each axis re-arms when the user lifts the key).
- `scenes/trainer_scene::update` calls
  `{tid_,sid_,money_}.on_focus_acquired()` when `list_.tick()` returns
  true and the destination is the matching numeric row.

`test_tid_roundtrip.py` now expects a clean `+30` from three RIGHT taps
(was `+29` with the bug). The latent issue is documented thoroughly in
the test docstring + the drive Lua header so a regression reads loudly.

**General lesson for CP#3+ widgets:** any focusable widget that reads
level-triggered hold state (`bn::keypad::*_held()`) inside a
`list_view`-driven scene must apply the same pattern. Your cycler
widget — if it does L/R hold-to-cycle — will hit the same trap on day
one. Mirror `numeric_field`'s arming pattern.

### G2 — `generate_fixtures.py` emits oversized Gen-3 fixtures

**Observed:** `libpksave/tests/fixtures/generate_fixtures.py` allocates a
131,072-byte buffer for Gen 3 but writes 278,520 bytes to disk. A
`bytearray[5:15] = bytes(20)`-style slice-assign somewhere is **growing**
the array, then the whole grown buffer is written.

**Why it matters:** `pksave_detect` returns `UNKNOWN` on any size other
than exactly 32 / 128 / 512 KiB. `test_gen3.c` already papers over this
by `fread`-ing exactly 128 KiB; the Python ctypes wrapper does the same
(`buf = raw[:128*1024]` in `test_decode_fixture.py`). The harness works,
but the fixture file itself is wrong and that's a footgun for any future
consumer.

**Resolved 2026-06-06 via Track B** — root cause at
`libpksave/tests/fixtures/generate_fixtures.py:697`:
`bytearray(GEN3_SIZE // GEN3_NUM_SECTIONS)` evaluated to 9,362 bytes
rather than the intended 4,096 (the in-line comment claimed the latter).
The slice-assign `buf[i*4096:(i+1)*4096] = sec` then grew the buffer by
5,266 bytes × 28 sections = 147,448 bytes. Fix uses the existing
`GEN3_SECTION_SIZE` constant and adds a pre-write size assertion in
`main()`. Regression guard: `libpksave/tests/test_fixtures.c` opens each
of the 12 fixture files and asserts `size == canonical`. Wired into
`make test`.

---

## Parallel work tracks

Each track is independently shippable in its own PR. Within a track,
work is sequential; between tracks, nothing is gated except where the
**Blocks** line says so. Pick whichever is unclaimed.

### ~~Track A~~ — **Resolved 2026-06-06.** Was a false gap (see G1 retraction). Shipped `test_flash_detected.py` as the regression guard.

- **Scope:** investigate why mGBA detects SRAM; fix the butano config or
  ROM data so `FLASH1M_Vnnn` ships in the cartridge.
- **Files (likely):** `frontend-gba/Makefile`, butano config in
  `frontend-gba/src/`, possibly a `.s` blob declaring the save-type
  string in a non-discardable section.
- **Validation:** rebuild `pksave-gba.gba`, then
  `docker run --rm -v $PWD:/src:ro pksave-test-gba mgba-lua /src/frontend-gba/pksave-gba.gba /opt/mgba-test/smoke.lua 10`
  should log `Detected FLASH savegame` (not SRAM).
- **Blocks:** H (round-trip decode), J (full round-trip).
- **Parallel with:** B, C, D, E.

### ~~Track B~~ — **Resolved 2026-06-06** (see G2 fix note). All fixtures correct-sized; 12-test Unity guard.

- **Scope:** find and fix the slice-assign that grows the Gen 3 fixture
  bytearray past 131,072. Regenerate all `libpksave/tests/fixtures/*.sav`.
  Add a Unity assertion in `test_gen3.c` (or a new `test_fixtures.c`)
  that the fixture files on disk are exactly the expected size, so this
  can't silently regrow.
- **Files:** `libpksave/tests/fixtures/generate_fixtures.py`,
  `libpksave/tests/fixtures/*.sav`, `libpksave/tests/test_gen3.c` or new.
- **Validation:** `stat -c%s libpksave/tests/fixtures/rs.sav` == 131072,
  same for `emerald.sav` and `frlg.sav`; `make test` still green.
- **Blocks:** nothing immediately (harness works around it), but lets us
  remove the `buf = raw[:128*1024]` truncation in
  `test_decode_fixture.py` and clean up future fixture-injection paths.
- **Parallel with:** A, C, D, E.

### ~~Track C~~ — **Resolved 2026-06-06.** Shipped `--save <path>` flag using `mCoreLoadSaveFile(core, savePath, true)`. Test in `test_sidecar_save.py`. Gotcha: mGBA opens the save file with `O_CREAT|O_RDWR` even in temporary mode, so the dump dir must be RW-mounted.

- **Scope:** add a third positional arg or `--save <path>` flag to
  `docker/test-gba/mgba-lua.c`. Before `core->reset(core)`, if the flag
  is set, call `mCoreLoadSaveFile(core, savePath, false)` (the second
  arg is `temporary`; `false` = persist writes). Document the new CLI
  in the Dockerfile header comment.
- **Files:** `docker/test-gba/mgba-lua.c`, `docker/test-gba/Dockerfile`.
- **Validation:** new Lua probe `probe_save_loaded.lua` reads byte 0 of
  SRAM after a known fixture is loaded and asserts it matches the
  fixture's byte 0 (e.g., the section-0 trainer-name first byte —
  pull from `libpksave/tests/fixtures/generate_fixtures.py`).
- **Blocks:** H, I, J.
- **Parallel with:** A, B, D, E.

### ~~Track D~~ — **Resolved 2026-06-06.** mGBA 0.10.5 exposes `emu:setKeys/addKeys/clearKeys(mask)`, `emu:addKey/clearKey(idx)`, with constants on `C.GBA_KEY.{A=0..L=9}` matching KEYINPUT bit ordering. KEYINPUT (`0x04000130`) reflects writes immediately, no frame advance required. Test in `test_key_input.py`.

- **Scope:** write a probe Lua script analogous to `probe_sram_read.lua`
  that enumerates whatever `emu` exposes for joypad / key input
  (`emu:setKeys`, `emu:addKeys`, `emu:clearKeys`, `emu.input`, etc.).
  Document findings inline in a `frontend-gba/tests/automation/INPUT.md`
  or extend this doc. If the upstream API is missing critical pieces
  (hold-N-frames helpers, button release timing), capture that here as
  a sub-gap rather than expanding mgba-lua.c yet — let the integrator
  (Track F) decide whether to wrap or extend.
- **Files:** `frontend-gba/tests/automation/probe_keys.lua`,
  short findings file or section.
- **Validation:** probe runs, output enumerates available accessors,
  one tap (e.g., A button for 1 frame) round-trips through whatever
  scene is on screen — frame-counter delta or a `console:log` from
  a hooked frame callback is enough.
- **Blocks:** I (input-driven scene drive), J.
- **Parallel with:** A, B, C, E.

### ~~Track E~~ — **Resolved 2026-06-06.** Shipped `dump_sram_full.lua` + `test_sram_dump_full.py`. mGBA 0.10.5 does **not** register `emu.memory.sram`; agent worked around by mirror-probing the region size (write marker byte, find lowest power-of-2 offset whose readback matches — that's the GBA address-mirror wrap point). `emu:read8` byte-loop dump is ~0.5s for 32 KiB and ~2s for 128 KiB. Caveat: like Track A's case, the dump observes whatever region mGBA has *currently* allocated, which is FLASH-vs-SRAM-decided by the editor's first cart-memory access. Tests that need the full 128 KiB blob should run for ≥~15 frames before probing.

- **Scope:** extend `dump_sram_head.lua` to dump the entire SRAM region.
  For FLASH 128 KiB that's 131,072 bytes. Use `emu.memory.sram:readRange`
  (after the warm-up byte read) if it returns a Lua string of the
  region; fall back to a `emu:read8` loop if not — the loop will be
  slow (~1-2s for 128 KiB) but acceptable for a test.
- **Files:** `frontend-gba/tests/automation/dump_sram_full.lua` (new),
  optional `test_sram_dump_full.py` to assert size.
- **Validation:** dump file is exactly 131,072 bytes; without a fixture
  loaded, contents are all `0xFF` (FLASH erased state) or all `0x00`
  (SRAM uninitialised) depending on whether Track A has landed.
- **Blocks:** H, J.
- **Parallel with:** A, B, C, D.

### ~~Track F~~ — **Resolved 2026-06-06.** `frontend-gba/tests/automation/helpers.lua` ships `M.at(frame, fn)` (frame-scheduled callbacks), `M.tap(key, frame, hold=3)`, `M.sram_read_all()`, `M.sram_dump(path)`, and the `M.KEY` mask table. `dump_after_boot.lua` and `drive_main_menu_a.lua` were refactored to consume it as part of gates H/I/J landing — the duplicated mirror-probe + byte-loop is now in one place.

- **Scope:** centralise the input + timing + dump primitives in one
  module (`helpers.lua`) so per-checkpoint Lua test scripts read like
  a sequence of editor actions, not a sequence of mGBA API calls.
  Reason: when butano debounce or mGBA frame timing changes, you tune
  helpers once, not N test scripts. Called out as a known risk in
  `docs/frontend-gba-plan.md` §"mGBA Lua automation".
- **Files:** `frontend-gba/tests/automation/helpers.lua`.
- **Depends on:** D (for input primitives), E (for dump), C (for
  fixture loading from the Python side). Any subset can ship first;
  helpers grow incrementally.
- **Blocks:** G (per-checkpoint cases), J.
- **Parallel with:** itself (multiple helpers can be written in
  parallel by different contributors once the file exists).

### Track G — Per-checkpoint Lua test cases

- **Scope:** for each ticked box in `frontend-gba/tests/checklist.md`,
  ship one Python test + one Lua script that mechanises the check.
  Plan §"Scope of full coverage" already lists this; this track
  formalises the mapping. Each test references its checklist row so the
  two stay synchronised.
- **Files:** `frontend-gba/tests/automation/test_cp<N>_<feature>.py`
  + sibling `cp<N>_<feature>.lua`, one pair per checklist row.
- **Depends on:** F (helpers).
- **Parallel with:** itself — each test case is independent of every
  other. Naturally distributes across whoever ships a checkpoint.

---

## Sequential gates (the round-trip itself)

These are not parallel tracks — they're combinations that land once
their prerequisite tracks are green.

### ~~H~~ — **Resolved 2026-06-06.** Sidecar load (Track C) → 120-frame run so FLASH unlock fires (lazy-detect lesson from G1) → mirror-probe full 128 KiB dump (Track E pattern) → ctypes decode (POC) → asserts `detect == RS` and `trainer.name == "RED"`. Passed first try.

- **File:** `frontend-gba/tests/automation/test_fixture_roundtrip.py` + `dump_after_boot.lua`.

### ~~I~~ — **Resolved 2026-06-06.** Boot, `h.tap(h.KEY.A, 30)`, run to shutdown, dump full FLASH, assert all 0xFF. Confirms navigation produces zero FLASH writes — matches the "FLASH rewritten only by Save & Exit" plan invariant.

- **File:** `frontend-gba/tests/automation/test_trainer_scene_nav.py` + `drive_main_menu_a.lua`.

### ~~J~~ — **Resolved 2026-06-06.** Full UI drive: `main_menu → trainer → text_input → 3×A → START → B → 4×DOWN → A (Save & Exit) → FLASH write`. Asserts `trainer.name == "REDAAA"` (original "RED" + 3 chars). **One catch the integrator should know:** the editor ROM `frontend-gba/pksave-gba.gba` is `.gitignore`-d and was stale in the working tree. Built from old object files where `text_input_scene::tab_` somehow defaulted to 2 (SYMBOL) — `A` placed `'0'` instead of `'A'`. Source's `tab_(0)` initializer was unchanged since 13beb9e; the binary just hadn't been rebuilt. **A clean `make gba` before running gate J is now load-bearing**; if the test starts failing with `'RED000'`-style output, it's a stale-ROM smell, not a butano logic regression.

- **File:** `frontend-gba/tests/automation/test_trainer_name_roundtrip.py` + `drive_name_edit_save.lua`.

---

## Dependency summary

```
A ──┐
B ──┼─ (independent of harness; library-side cleanup)
C ──┤
D ──┤     ┌─ I ─┐
E ──┘     │     │
A+C+E ── H      ├── J
D+F ──── I ─────┘
A+B+C+D+E ─ F ─ G (per-checkpoint coverage, ongoing)
```

- A, B, C, D, E are five independent picks. None blocks another.
- F integrates as A/C/D/E land.
- G runs in parallel with itself once F exists.
- H, I, J are the gates that prove the round-trip story is whole.

---

## Framebuffer dump — added 2026-06-10

Closes the only remaining `make test-gba` checkbox under
`docs/frontend-gba-plan.md` §"Acceptance criteria" (dirty indicator
appears on edit, clears on save). The infrastructure is small enough
to live in this section rather than its own track / gate.

**Why it took new C code in `mgba-lua.c`:** mGBA's built-in
`emu:screenshot(path)` calls `core->getPixels`, which only works once
the software renderer has been wired up via `core->setVideoBuffer`.
The Qt/SDL frontends do that automatically; mgba-lua never did, so
`emu:screenshot` segfaulted on the dummy renderer's no-op pixels read
(the dummy leaves `*pixels` untouched — see `src/gba/video.c
GBAVideoDummyRendererGetPixels`). Fix is a one-line allocate +
`setVideoBuffer(core, buf, width)` BEFORE `core->reset(core)`; the GBA
core's reset path then swaps the dummy renderer for the software one
(`src/gba/core.c _GBACoreReset`, line ~598). After that emu:screenshot
also works (verified writing valid PNGs).

**Why a custom `fb_dump_ppm` and not just emu:screenshot:** the harness
runs on the bare stdlib (no Pillow), and a stdlib PNG decoder would
mean pulling zlib + a 100-line scanline-unfilter routine into
`framebuffer.py`. Binary PPM (P6) is a 3-line ASCII header followed by
raw RGB triples — `re.match` + slice. The custom function is bound via
`mSCRIPT_BIND_FUNCTION` + `mScriptContextSetGlobal("fb_dump_ppm", ...)`
in `mgba-lua.c` so Lua sees a global function returning 0 on success.

**Lua-side wrapper:** `helpers.lua` exposes `M.fb_dump(path)` which
calls `fb_dump_ppm(path)`, logs success, and surfaces any negative
error code from C.

**Python-side helpers:** `framebuffer.py` provides:
- `Framebuffer.load(path)` — PPM parse → `(width, height, pixels)`
- `.region(x, y, w, h)` / `.region_hash(...)` — strip extraction + hash
- `.count_non_black(...)` — proxy for "how much ink is drawn here"
- `header_strip_hash(fb)` / `header_strip_non_black(fb)` — geometry
  pre-set to the `main_menu_scene` header band (y=0..32). Refactor
  if any other scene wants its own strip helper.

**Test that consumes it:** `test_dirty_indicator.py` — snapshots
CLEAN/DIRTY/AFTER_SAVE, asserts hash flip + hash restoration. The
AFTER_SAVE hash must be **byte-equal** to CLEAN, which doubles as a
weak regression check on butano text rendering: a font/glyph metric
change shows up here as a mismatch.

**Caveat for new framebuffer tests:**
- Snapshots are tied to mgba frame counts, not scene state. The drive
  script's `h.at(N, fn)` fires at frame N regardless of whether the
  expected scene has finished rendering. Budget at least 30 frames of
  slack after each input event before snapshotting, and bump
  `FRAMES=` in the Python test if you add later snapshots.
- The header strip is centred text (`text_generator_.set_center_alignment()`),
  so adding characters SHIFTS existing glyphs. Don't try to assert on
  a narrow column expecting the original characters to still be there.
  Hash the whole strip width or skip the assertion.

## Out of scope for this doc

- Loading the post-edit `.sav` into an **unmodified Pokémon ROM** to
  verify the trainer card renders the edit — v1 follow-up.
- Visual / palette / sprite-tearing regressions beyond the dirty-
  indicator strip — possible now that fb_dump_ppm exists, but each new
  visual assertion needs its own region geometry + a fresh decision on
  what to hash vs. what to count.
- NDS-frontend equivalents. The Gen IV developer inherits this harness
  pattern as precedent, not as a directly-reusable artefact.
