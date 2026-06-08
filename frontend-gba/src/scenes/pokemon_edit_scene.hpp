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
// IDENTITY tab rows (Wave 2B adds ROW_POKERUS):
//   0  ROW_SPECIES   cycler [1, 411]
//   1  ROW_NICKNAME  text_input drill (max 10)
//   2  ROW_OT_NAME   text_input drill (max 7)
//   3  ROW_ITEM      held-item cycler [0, 376]
//   4  ROW_ABILITY   ability cycler filtered to species' two slots
//   5  ROW_NATURE    nature cycler [0, 24]
//   6  ROW_SHINY     toggle (functional via L2 PID recompute)
//   7  ROW_POKERUS   drill into pokerus_modal_scene (A enters; modal
//                    owns commit/cancel)
//
// STATS tab rows (Wave 2B):
//   0      ROW_LEVEL  numeric [1, 100]
//   1..6   ROW_EV_HP..ROW_EV_SPE numeric [0, ev_max_per_stat(game)]
//   7..12  ROW_IV_HP..ROW_IV_SPE numeric [0, iv_max(game)]
//
// MOVES tab rows (Wave 2C): 4 (move, PP) pairs = 8 rows.
//   0  ROW_MOVE_0   cycler [0, 354]   (id 0 = empty slot)
//   1  ROW_PP_0     numeric [0, floor(base_pp(move_ids[0]) * 1.6)]
//   2  ROW_MOVE_1   cycler
//   3  ROW_PP_1     numeric
//   4  ROW_MOVE_2   cycler
//   5  ROW_PP_2     numeric
//   6  ROW_MOVE_3   cycler
//   7  ROW_PP_3     numeric
//
// Move id 0 renders as "(empty)" and the paired PP row renders as
// "PP N: -/-".  When the move cycles from 0 → non-zero, PP is seeded
// to its new max.  When it cycles to 0, PP is zeroed.  Otherwise the
// PP cap is reseated via numeric_field::set_max() and current PP is
// clamped down if it exceeds the new cap (Hyper Beam → Tackle leaves
// PP alone; Tackle → Hyper Beam clamps it).
//
// STATS tab EV total-cap (510 in Gen 3) is currently NOT enforced at
// edit time — per-stat caps are enforced; the right panel shows
// "EV total: NNN / 510" so the user sees overrun.  Total-cap
// enforcement is a Wave 3 follow-up.

#ifndef PKSAVE_GBA_POKEMON_EDIT_SCENE_HPP
#define PKSAVE_GBA_POKEMON_EDIT_SCENE_HPP

#include "scene.hpp"
#include "pksave_bridge.hpp"
#include "widgets/cycler.hpp"
#include "widgets/list_view.hpp"
#include "widgets/numeric_field.hpp"

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

// Where this Pokemon lives in the save — selects party vs box APIs in
// load_from_save / push_to_save, and which scene B returns to.  The
// ctor decides this by checking which pending-* pair on the bridge is
// set; box wins if pending_box_index() >= 0 (the bag pattern), else
// party fallback via pending_party_slot().
enum class edit_source {
    PARTY,
    BOX,
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

    // Returns the numeric_field driving a given STATS row, or nullptr
    // if the row isn't a numeric edit row (out of range, or some other
    // tab).  Used by the STATS active-edit tick path.
    numeric_field* stats_field_for_row(int row);

    // Push the just-edited stats row back into pokemon_ and run a
    // pksave_recalc_stats() so the right panel reflects the new
    // calculated stats in the same frame.
    void commit_stats_row(int row);

    // MOVES tab helpers.  Slot is the 0..3 move-slot index.
    int            move_slot_for_row(int row) const;     // -1 if PP row
    int            pp_slot_for_row(int row) const;       // -1 if move row
    int            pp_cap_for_slot(int slot) const;      // floor(base*1.6)
    cycler*        move_cycler_for_slot(int slot);
    numeric_field* pp_field_for_slot(int slot);
    void           commit_move_slot(int slot);
    void           commit_pp_slot(int slot);

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

    // STATS-tab numeric fields.  Per-stat caps come from libpksave
    // (pksave_ev_max_per_stat / pksave_iv_max); ranges are seeded in
    // the ctor and re-seeded on tab entry via rebuild_for_current_tab().
    numeric_field    level_;
    numeric_field    ev_hp_;
    numeric_field    ev_atk_;
    numeric_field    ev_def_;
    numeric_field    ev_spa_;
    numeric_field    ev_spd_;
    numeric_field    ev_spe_;
    numeric_field    iv_hp_;
    numeric_field    iv_atk_;
    numeric_field    iv_def_;
    numeric_field    iv_spa_;
    numeric_field    iv_spd_;
    numeric_field    iv_spe_;

    // MOVES-tab widgets — one cycler per move slot, one numeric per PP.
    // PP cap is reseated on move id changes via numeric_field::set_max.
    cycler           move_0_;
    cycler           move_1_;
    cycler           move_2_;
    cycler           move_3_;
    numeric_field    pp_0_;
    numeric_field    pp_1_;
    numeric_field    pp_2_;
    numeric_field    pp_3_;

    pksave_pokemon_t pokemon_;
    edit_source      source_;
    int              slot_;
    int              box_;   // valid iff source_ == BOX

    edit_tab         tab_;
    edit_mode        mode_;
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_POKEMON_EDIT_SCENE_HPP
