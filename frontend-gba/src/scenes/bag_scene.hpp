// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Bag screen — pocket-tabbed slot list.
//
// Layout (CP#4):
//   Header     : "BAG <POCKET>"
//   Tab row    : ITEMS | BALLS | TM/HM | KEY | BERRIES
//                 — focused pocket highlighted with brackets [BALLS]
//   Slot list  : list_view over the focused pocket's filled slots,
//                empty slots rendered as "[Empty]" at the end.
//
// D-pad grammar (per docs/frontend-gba-plan.md §5):
//   L / R       : (reserved for top-level page cycling — no-op here)
//   D-pad L/R   : cycle pocket tab (saturating; does NOT wrap, matching
//                 cycler/pocket-tab convention in pokemon_edit_scene)
//   D-pad U/D   : scroll the slot list (via list_view)
//   A           : drill into slot_edit_scene for the selected slot;
//                 records (pocket, slot) on the bridge for handoff
//   B           : back to main_menu
//
// On re-entry from slot_edit_scene (bridge_.clear_pending_bag() makes the
// transition idempotent), we reload the current pocket so the user sees
// their edit reflected.

#ifndef PKSAVE_GBA_BAG_SCENE_HPP
#define PKSAVE_GBA_BAG_SCENE_HPP

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

class bag_scene final : public scene {
public:
    explicit bag_scene(pksave_bridge& bridge);

    bn::optional<scene_kind> update() override;

private:
    void load_pocket();
    void redraw_header();
    void redraw_tabs();

    static void row_provider_thunk(void* ctx, int row, bn::string<32>& out);
    void format_row(int row, bn::string<32>& out);

    pksave_bridge&            bridge_;
    bn::sprite_text_generator text_gen_;
    bn::vector<bn::sprite_ptr, 32> header_sprites_;
    bn::vector<bn::sprite_ptr, 48> tab_sprites_;

    // Worst-case Gen-3 RSE TM/HM pocket has 64 slots; size for that.
    static constexpr int MAX_SLOTS = 64;
    pksave_item_t items_[MAX_SLOTS];
    uint8_t       item_count_;

    // 0..4 inclusive — index into pocket_order_[] (not a pksave_pocket_t).
    int pocket_index_;

    list_view list_;
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_BAG_SCENE_HPP
