// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Box screen (CP#5) — linear slot list over one box at a time.
//
// Per plan §10:
//   - Linear 30-slot list reusing party_scene's list_view, no grid widget.
//   - Header shows "BOX N  filled/30" plus the dirty "*" indicator.
//   - SELECT cycles boxes (wraps).  D-pad LEFT/RIGHT reserved (would
//     cycle at column boundaries in a grid layout; the linear list has
//     no such concept, so they're no-ops here).
//   - A drills into pokemon_edit_scene with (box, slot) recorded on the
//     bridge as the pending-edit handoff.  Drilling is allowed on empty
//     rows: edits land via pksave_set_box_slot which handles the "create
//     new slot from empty" path (species 0 → non-zero) the same way as
//     party.
//   - B returns to main_menu.
//
// On re-entry from pokemon_edit_scene (bridge_.clear_pending_box() makes
// the transition idempotent — same pattern as bag_scene), we reload the
// current box so the user sees their edit reflected.

#ifndef PKSAVE_GBA_BOX_SCENE_HPP
#define PKSAVE_GBA_BOX_SCENE_HPP

#include "scene.hpp"
#include "pksave_bridge.hpp"
#include "widgets/list_view.hpp"

#include <bn_sprite_text_generator.h>
#include <bn_sprite_ptr.h>
#include <bn_string.h>
#include <bn_vector.h>

extern "C" {
#include "pksave.h"
}

namespace pksave_gba {

class box_scene final : public scene {
public:
    explicit box_scene(pksave_bridge& bridge);

    bn::optional<scene_kind> update() override;

private:
    void load_box();
    void redraw_header();

    static void row_provider_thunk(void* ctx, int row, bn::string<32>& out);
    void format_row(int row, bn::string<32>& out);

    pksave_bridge&            bridge_;
    bn::sprite_text_generator text_gen_;
    bn::vector<bn::sprite_ptr, 32> header_sprites_;

    list_view        list_;

    static constexpr int SLOTS_PER_BOX = 30;
    pksave_pokemon_t box_[SLOTS_PER_BOX];
    uint8_t          box_count_;     // filled slots in box_
    int              box_index_;     // 0 .. box_total_ - 1
    int              box_total_;     // libpksave-reported count (Gen 3: 14)
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_BOX_SCENE_HPP
