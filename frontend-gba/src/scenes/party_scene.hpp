// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Party screen — read-only list of the 6 party slots.
//
// Row layout (driven by list_view, one row per slot):
//   filled slot : "{species_name} L{level} {nickname}"
//   empty slot  : "[Empty]"
//
// Input grammar:
//   - up/down  : list_view navigation
//   - A        : drill into pokemon_edit_scene (CP#3-C wires the
//                actual transition).  For CP#3-B we stash the chosen
//                slot index in pksave_bridge::pending_party_slot() and
//                return scene_kind::PARTY (no-op transition); CP#3-C
//                will replace that with scene_kind::POKEMON_EDIT and
//                consume the stashed slot in pokemon_edit_scene's ctor.
//   - B        : return to main_menu.
//
// Choice for the drill stub (Option A from AGENT_PROMPT):
//   - Stash slot index in pksave_bridge, return scene_kind::PARTY now.
//   - Rejected Option B (add POKEMON_EDIT to scene_kind + a placeholder
//     scene that bounces back to PARTY): more moving parts to delete
//     once CP#3-C lands than a one-line return change here.

#ifndef PKSAVE_GBA_PARTY_SCENE_HPP
#define PKSAVE_GBA_PARTY_SCENE_HPP

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

class party_scene final : public scene {
public:
    explicit party_scene(pksave_bridge& bridge);

    bn::optional<scene_kind> update() override;

private:
    void load_from_save();
    void redraw_header();

    static void row_provider_thunk(void* ctx, int row, bn::string<32>& out);
    void format_row(int row, bn::string<32>& out);

    pksave_bridge&            bridge_;
    bn::sprite_text_generator text_gen_;
    bn::vector<bn::sprite_ptr, 32> header_sprites_;

    list_view        list_;
    pksave_pokemon_t party_[6];
    uint8_t          party_count_;
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_PARTY_SCENE_HPP
