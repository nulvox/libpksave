// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Bag slot editor — drilled from bag_scene with (pocket, slot) recorded
// on the bridge.
//
// Two rows (always-live, no NAV/ACTIVE gate; there are no tabs to
// disambiguate against):
//   ROW_ITEM  : item cycler scoped via pksave_item_pocket() to the
//               handoff pocket.  Stepping the cycler updates the qty
//               field's max from pksave_item_max_stack().
//   ROW_QTY   : numeric_field; cap from L6 (1 for HMs / key items,
//               99 for stackables, 999 for berries).  ID 0 ("none") has
//               cap 0 so the user can't put a quantity on an empty slot.
//
// D-pad grammar:
//   UP / DOWN  : list_view navigation between the two rows.
//   LEFT/RIGHT : on ROW_ITEM → cycler step; on ROW_QTY → ±10 qty.
//   A          : commit pksave_set_item(handoff_pocket, handoff_slot,
//                {item, qty}); mark bridge dirty; return BAG.
//   B          : cancel; return BAG.
//
// On entry, the scene snapshots the current slot via pksave_get_pocket so
// the cycler/qty seed from the live save state.

#ifndef PKSAVE_GBA_SLOT_EDIT_SCENE_HPP
#define PKSAVE_GBA_SLOT_EDIT_SCENE_HPP

#include "scene.hpp"
#include "pksave_bridge.hpp"
#include "widgets/list_view.hpp"
#include "widgets/cycler.hpp"
#include "widgets/numeric_field.hpp"

#include <bn_sprite_text_generator.h>
#include <bn_sprite_ptr.h>
#include <bn_string.h>
#include <bn_vector.h>

extern "C" {
#include "pksave.h"
}

namespace pksave_gba {

class slot_edit_scene final : public scene {
public:
    explicit slot_edit_scene(pksave_bridge& bridge);

    bn::optional<scene_kind> update() override;

private:
    void load_slot();
    void redraw_header();
    void resync_qty_max();  // call after cycler.id() changes

    static void row_provider_thunk(void* ctx, int row, bn::string<32>& out);
    void format_row(int row, bn::string<32>& out);

    static bool pocket_filter_thunk(void* ctx, int id);

    pksave_bridge&            bridge_;
    bn::sprite_text_generator text_gen_;
    bn::vector<bn::sprite_ptr, 32> header_sprites_;

    list_view     list_;
    cycler        item_;
    numeric_field qty_;

    pksave_pocket_t pocket_;  // snapshot from bridge_ at ctor
    int             slot_;    // snapshot from bridge_ at ctor
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_SLOT_EDIT_SCENE_HPP
