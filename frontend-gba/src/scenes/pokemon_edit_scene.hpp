// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Pokemon editor — Checkpoint #3 tracer-bullet slice.
//
// Final shape (plan §"Locked architectural decisions" #9) has three
// tabs (IDENTITY / STATS / MOVES) with D-pad left/right cycling between
// tabs when no field is in active-edit mode.  This file implements only
// the smallest end-to-end slice that proves the round-trip works:
//
//   - Tab indicator at the top: "IDENTITY  stats  moves" — IDENTITY
//     highlighted, the other two rendered dim.  D-pad left/right tab
//     navigation is intentionally a NO-OP in this slice (TODO: wave 2).
//     The IDENTITY tab here has no active-edit mode gate, and the
//     species cycler always reads L/R; we'll introduce the active-edit
//     gate when we add fields whose L/R-as-tab-cycle would conflict.
//   - IDENTITY tab with ONE editable field: species cycler (D-pad
//     LEFT/RIGHT cycle, range [1, 411]).  format_row renders
//     "Species: <name>" — the cycler widget owns no glyphs, so the
//     ASCII-fallback arrows are absorbed into the row label.
//   - Right-panel anchor: three rows of text — "[?]" silhouette
//     placeholder, species name, "L<level>".  Re-rendered after every
//     species edit.
//
// Input grammar:
//   - up/down  : list_view navigation (one row in this slice).
//   - left/right : species cycler tick when ROW_SPECIES focused.
//                  Note: tab-cycling is left/right too; the locked
//                  decision in §9 is that left/right cycles tabs when
//                  NO field is in active-edit mode.  This slice has
//                  no active-edit gate yet, so left/right always feeds
//                  the cycler.  Wave 2 introduces the gate.
//   - A : no-op on ROW_SPECIES in this slice (cycler is always live).
//   - B : return to PARTY.
//
// Bridge handoff: the slot index is consumed from
// bridge.pending_party_slot() in the ctor (set by party_scene's A
// handler).  The slot index is not re-read after construction — the
// editor mutates its own in-memory pokemon_ copy and pushes via
// pksave_set_party_slot on every edit.

#ifndef PKSAVE_GBA_POKEMON_EDIT_SCENE_HPP
#define PKSAVE_GBA_POKEMON_EDIT_SCENE_HPP

#include "scene.hpp"
#include "pksave_bridge.hpp"
#include "widgets/cycler.hpp"
#include "widgets/list_view.hpp"

#include <bn_sprite_text_generator.h>
#include <bn_sprite_ptr.h>
#include <bn_string.h>
#include <bn_vector.h>

extern "C" {
#include "pksave.h"
}

namespace pksave_gba {

class pokemon_edit_scene final : public scene {
public:
    explicit pokemon_edit_scene(pksave_bridge& bridge);

    bn::optional<scene_kind> update() override;

private:
    void load_from_save();
    void push_to_save();
    void redraw_header();
    void redraw_right_panel();

    static void row_provider_thunk(void* ctx, int row, bn::string<32>& out);
    void format_row(int row, bn::string<32>& out);

    pksave_bridge&            bridge_;
    bn::sprite_text_generator text_gen_;
    bn::vector<bn::sprite_ptr, 32> header_sprites_;
    bn::vector<bn::sprite_ptr, 32> right_panel_sprites_;

    list_view        list_;
    cycler           species_;

    pksave_pokemon_t pokemon_;
    int              slot_;
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_POKEMON_EDIT_SCENE_HPP
