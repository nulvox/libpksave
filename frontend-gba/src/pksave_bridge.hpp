// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// RAII wrapper around `pksave_save_t *` plus the 128KB save buffer
// that lives in EWRAM.  The buffer is a single static array; only one
// pksave_bridge instance may exist at a time (enforced at runtime via
// a counter).
//
// Why a static buffer (rather than a heap allocation): butano gives us
// 256KB of EWRAM and its own allocator runs at the top — we want the
// 128KB save buffer in a fixed location so an OOM surprise from
// butano's heap can never trash it.  Marking the array `static` at
// namespace scope puts it in .bss, which butano's startup zeroes for us.

#ifndef PKSAVE_GBA_PKSAVE_BRIDGE_HPP
#define PKSAVE_GBA_PKSAVE_BRIDGE_HPP

#include <cstdint>

extern "C" {
#include "pksave.h"
}

namespace pksave_gba {

// Access the singleton save buffer.  Defined in pksave_bridge.cpp;
// guaranteed to live at a stable address from program start to exit.
std::uint8_t* save_buffer();

class pksave_bridge {
public:
    // Adopt the already-loaded `save_buffer()` contents:
    //  - calls pksave_detect on the buffer
    //  - if game != UNKNOWN, calls pksave_open
    //  - stores the resulting handle (may be null)
    //
    // After construction, callers check ok() to see whether the buffer
    // was usable.  If not, .game() returns PKSAVE_GAME_UNKNOWN and
    // .reason() returns a short human-readable string suitable for
    // boot_error_scene.
    pksave_bridge();

    ~pksave_bridge();

    pksave_bridge(const pksave_bridge&)            = delete;
    pksave_bridge& operator=(const pksave_bridge&) = delete;

    // True iff pksave_open returned a non-null handle.
    bool ok() const { return handle_ != nullptr; }

    // PKSAVE_GAME_* (UNKNOWN if detection failed).
    pksave_game_t game() const { return game_; }

    // Human-readable reason string when !ok().  Returns nullptr if ok.
    const char* reason() const { return ok() ? nullptr : reason_; }

    // True iff save_buffer() has been mutated since last flush.
    // Frontend code calls mark_dirty() after every successful edit.
    bool is_dirty() const { return dirty_; }
    void mark_dirty() { dirty_ = true; }

    // Finalise checksums via pksave_flush, then rewrite FLASH from the
    // EWRAM buffer.  Clears the dirty flag on success.  Returns true on
    // success.  Caller is responsible for displaying the blocking
    // "DON'T TURN OFF THE POWER!" screen during the call.
    bool flush_to_flash();

    // Raw access for callers that need to feed the handle directly to
    // libpksave APIs (trainer/party/box/bag getters and setters).
    pksave_save_t* handle() const { return handle_; }

    // CP#3-B → CP#3-C handoff: party_scene records the slot the user
    // pressed A on; pokemon_edit_scene (CP#3-C) will read it from its
    // ctor to know which party slot to edit.  Until CP#3-C lands the
    // edit scene, this field is set but never consumed.  -1 means "no
    // selection pending".
    int  pending_party_slot() const { return pending_party_slot_; }
    void set_pending_party_slot(int slot) { pending_party_slot_ = slot; }

    // CP#4: bag_scene → slot_edit_scene handoff.  bag_scene records the
    // pocket and slot the user pressed A on; slot_edit_scene reads them
    // from its ctor to know what to edit.  Default -1 / NONE means "no
    // selection pending".  slot_edit_scene clears them on return so a
    // re-entry to bag_scene doesn't immediately re-drill.
    pksave_pocket_t pending_bag_pocket() const { return pending_bag_pocket_; }
    int             pending_bag_slot()   const { return pending_bag_slot_; }
    void set_pending_bag(pksave_pocket_t pocket, int slot)
    {
        pending_bag_pocket_ = pocket;
        pending_bag_slot_   = slot;
    }
    void clear_pending_bag()
    {
        pending_bag_pocket_ = PKSAVE_POCKET_NONE;
        pending_bag_slot_   = -1;
    }

private:
    pksave_save_t* handle_;
    pksave_game_t  game_;
    const char*    reason_;
    bool           dirty_;
    int            pending_party_slot_ = -1;
    pksave_pocket_t pending_bag_pocket_ = PKSAVE_POCKET_NONE;
    int             pending_bag_slot_   = -1;
};

// Friendly display name for the detected game variant — Checkpoint #1
// only needs the three Gen III banners.  Falls back to "Unknown" for
// any non-Gen-III value so callers can render something instead of
// crashing.
const char* game_display_name(pksave_game_t game);

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_PKSAVE_BRIDGE_HPP
