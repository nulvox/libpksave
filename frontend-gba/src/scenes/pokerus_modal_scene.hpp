// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Pokerus sub-modal — Wave 2B.
//
// A small modal opened from pokemon_edit_scene's IDENTITY tab when the
// user presses A on ROW_POKERUS.  Lets the user edit the slot's Pokerus
// strain / days and commit (A) or cancel (B).  On commit the scene
// returns to POKEMON_EDIT; on cancel it returns to POKEMON_EDIT without
// writing to the save.  Reads pokerus state via pksave_get_party_pokerus
// on construct; writes via pksave_set_party_pokerus on commit.
//
// Input grammar (deliberately simpler than pokemon_edit_scene's
// NAV/ACTIVE gate):
//
//   - D-pad UP/DOWN : move cursor between visible rows.
//   - D-pad L/R     : cycle the focused field's value (no ACTIVE gate;
//                     the modal has no tab-y context to disambiguate
//                     against).
//   - A             : commit pending strain/days via
//                     pksave_set_party_pokerus and return to POKEMON_EDIT.
//   - B             : cancel; return to POKEMON_EDIT without writing.
//
// Rows:
//   0 ROW_STATUS : cycler over NEVER / ACTIVE / CURED.
//   1 ROW_STRAIN : numeric in [1, 15].  Hidden when status == NEVER.
//   2 ROW_DAYS   : numeric in [0, 15].  Visible only when status == ACTIVE
//                  (CURED implies days = 0; NEVER implies days = 0).
//
// Status-cycle side effects (mirror the pokerus state machine in
// pksave.h):
//   - NEVER → ACTIVE   : if strain == 0 then strain = 1, days = 1.
//   - ACTIVE → CURED   : days = 0 (the cured state means days has
//                                  counted down).
//   - CURED → NEVER    : strain = 0 (formally clears infection).
//   - NEVER → CURED via wrap (UP from NEVER, L on cycler): same as the
//                              ACTIVE/CURED transition for safety —
//                              seed strain=1.
//
// Slot reuse: pokemon_edit_scene re-sets bridge_.pending_party_slot()
// before returning POKERUS_MODAL so this scene can read the same slot
// from the bridge.  See pokemon_edit_scene.cpp.

#ifndef PKSAVE_GBA_POKERUS_MODAL_SCENE_HPP
#define PKSAVE_GBA_POKERUS_MODAL_SCENE_HPP

#include "scene.hpp"
#include "pksave_bridge.hpp"

#include <bn_sprite_text_generator.h>
#include <bn_sprite_ptr.h>
#include <bn_string.h>
#include <bn_vector.h>

extern "C" {
#include "pksave.h"
}

namespace pksave_gba {

class pokerus_modal_scene final : public scene {
public:
    explicit pokerus_modal_scene(pksave_bridge& bridge);

    bn::optional<scene_kind> update() override;

private:
    void load_from_save();
    void commit_to_save();
    void redraw();

    // Cycle status NEVER → ACTIVE → CURED → NEVER (wrap).  `direction`
    // is +1 (RIGHT) or -1 (LEFT).
    void cycle_status(int direction);

    // Step strain / days by `delta`, saturating at their range.
    void step_strain(int delta);
    void step_days(int delta);

    // Which rows are visible given the current status.
    int row_count_visible() const;

    pksave_bridge&            bridge_;
    bn::sprite_text_generator text_gen_;
    bn::vector<bn::sprite_ptr, 32> sprites_;

    int                       slot_;
    pksave_pokerus_status_t   status_;
    uint8_t                   strain_;
    uint8_t                   days_;

    // 0 = STATUS, 1 = STRAIN, 2 = DAYS
    int                       cursor_;

    // L/R debounce — read pressed-edges, not held, to keep the cycle
    // grammar predictable in the small modal (no fast-cycle here).
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_POKERUS_MODAL_SCENE_HPP
