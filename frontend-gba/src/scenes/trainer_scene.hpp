// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Trainer screen — name, TID, SID, money, and eight badge toggles.
//
// Field layout (driven by list_view):
//   row 0: Name      (drills into text_input)
//   row 1: TID       (numeric_field 0..65535)
//   row 2: SID       (numeric_field 0..65535)
//   row 3: Money     (numeric_field 0..999999)
//   rows 4..11: Badge 1..Badge 8 (toggle)
//
// Per-Checkpoint-#2 simplification (per AGENT_PROMPT.md):
//   Numeric and toggle rows are "always editable when focused" — no
//   explicit active-edit toggle.  D-pad up/down moves selection only
//   when the focused row is a numeric field whose left/right axis
//   would otherwise be used to step ±10; we resolve the conflict by
//   reserving D-pad up/down for navigation regardless of focus, and
//   D-pad left/right (and the special-case up/down on numerics that
//   happen to coincide) routes through the numeric_field.  Concretely:
//     - up/down  : navigate list (always)
//     - left/right when numeric row focused : adjust by ±10 with accel
//     - A on Name row    : open text_input
//     - A on numeric row : no-op (numeric rows have no active-edit gate
//                          in CP#2)
//     - A on badge row   : toggle the bit
//     - B : return to main menu
//
// This conflicts with the "up/down = +/-1" half of the numeric_field
// spec.  Checkpoint #3's cycler-vs-numeric ambiguity forces an
// active-edit-mode gate; for CP#2 we ship the simpler shape and accept
// that numeric fields only have left/right ±10 here.  The numeric_field
// widget itself still supports the up/down ±1 path — that's reused
// later when active-edit mode lands.

#ifndef PKSAVE_GBA_TRAINER_SCENE_HPP
#define PKSAVE_GBA_TRAINER_SCENE_HPP

#include "scene.hpp"
#include "pksave_bridge.hpp"
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

class trainer_scene final : public scene {
public:
    explicit trainer_scene(pksave_bridge& bridge);

    bn::optional<scene_kind> update() override;

private:
    void load_from_save();
    void push_to_save();
    void redraw_header();

    // list_view row-provider trampoline.
    static void row_provider_thunk(void* ctx, int row, bn::string<32>& out);
    void format_row(int row, bn::string<32>& out);

    // Handle a returning text_input result (consumed on entry if valid).
    void consume_text_input_result();

    pksave_bridge&            bridge_;
    bn::sprite_text_generator text_gen_;
    bn::vector<bn::sprite_ptr, 32> header_sprites_;

    list_view     list_;
    pksave_trainer_t trainer_;

    // Per-field numeric editors (constructed once from the loaded trainer).
    numeric_field tid_;
    numeric_field sid_;
    numeric_field money_;

    // Which field the user is currently editing — the same as list_.selected()
    // but cached so format_row can highlight differently.  Updated each frame.
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_TRAINER_SCENE_HPP
