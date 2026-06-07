// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Pokemon editor — three sub-tabs (IDENTITY / STATS / MOVES) per plan
// §9.  D-pad LEFT/RIGHT cycles tabs when no field is in active-edit
// mode (NAV); cycles the focused field's value when in ACTIVE mode.
// A on a row that supports it enters ACTIVE; A inside ACTIVE commits
// and returns to NAV.  B inside ACTIVE returns to NAV without revert
// (no revert path in Wave 2A — documented intentional deviation).
//
// IDENTITY tab rows (Wave 2A scope):
//   0  ROW_SPECIES   cycler [1, 411]
//   1  ROW_NICKNAME  text_input drill (max 10)
//   2  ROW_OT_NAME   text_input drill (max 7)
//   3  ROW_ITEM      held-item cycler [0, 376]
//   4  ROW_ABILITY   ability cycler filtered to species' two slots
//   5  ROW_NATURE    nature cycler [0, 24]
//   6  ROW_SHINY     toggle (functional via L2 PID recompute)
//
// STATS and MOVES tabs render placeholder text in Wave 2A; their field
// content lands in Wave 2B / 2C.
//
// Pokerus sub-modal: deferred to Wave 2B per the plan.

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

enum class edit_tab : int {
    IDENTITY = 0,
    STATS    = 1,
    MOVES    = 2,
};

enum class edit_mode {
    NAV,
    ACTIVE,
};

// What text-input target is currently being drilled into.  Encoded as
// the mailbox's caller_tag so the result survives the scene-reset
// across the TEXT_INPUT → POKEMON_EDIT transition.
enum class edit_text_target : int {
    NONE     = 0,
    NICKNAME = 1,
    OT_NAME  = 2,
};

class pokemon_edit_scene final : public scene {
public:
    explicit pokemon_edit_scene(pksave_bridge& bridge);

    bn::optional<scene_kind> update() override;

private:
    void load_from_save();
    void push_to_save();
    void redraw_header();
    void redraw_right_panel();
    void rebuild_for_current_tab();

    void consume_text_input_result();
    void open_text_input_for(edit_text_target target);

    void enter_active();
    void exit_active();

    static void row_provider_thunk(void* ctx, int row, bn::string<32>& out);
    void format_row(int row, bn::string<32>& out);

    // Ability cycler filter: returns true iff `id` matches either of
    // the current species' ability slots.
    static bool ability_filter_thunk(void* ctx, int id);

    int row_count_for_tab() const;

    pksave_bridge&            bridge_;
    bn::sprite_text_generator text_gen_;
    bn::vector<bn::sprite_ptr, 32> header_sprites_;
    bn::vector<bn::sprite_ptr, 32> right_panel_sprites_;
    bn::vector<bn::sprite_ptr, 16> placeholder_sprites_;

    list_view        list_;
    cycler           species_;
    cycler           item_;
    cycler           ability_;
    cycler           nature_;

    pksave_pokemon_t pokemon_;
    int              slot_;

    edit_tab         tab_;
    edit_mode        mode_;
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_POKEMON_EDIT_SCENE_HPP
