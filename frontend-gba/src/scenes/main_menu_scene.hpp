// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Top-level menu: Trainer / Party / Bag / Boxes / Save & Exit.
//
// Checkpoint #1 behaviour:
//   - D-pad up/down moves the selection cursor.
//   - A on an entry maps to the corresponding scene_kind — but target
//     scenes don't exist yet, so update() returns bn::nullopt instead
//     of actually transitioning.  The mapping itself IS wired so that
//     Checkpoint #2 only needs to flip the return statement.
//   - L/R shoulders are reserved for cycling top-level pages.  Since
//     other pages don't exist yet, they are no-ops in this checkpoint.
//   - Header shows the detected game name and a "*" dirty indicator
//     when bridge.is_dirty() — won't appear in Checkpoint #1 since
//     nothing mutates yet, verified by the manual checklist.

#ifndef PKSAVE_GBA_MAIN_MENU_SCENE_HPP
#define PKSAVE_GBA_MAIN_MENU_SCENE_HPP

#include "scene.hpp"
#include "pksave_bridge.hpp"

#include <bn_sprite_text_generator.h>
#include <bn_vector.h>
#include <bn_sprite_ptr.h>

namespace pksave_gba {

class main_menu_scene final : public scene {
public:
    explicit main_menu_scene(pksave_bridge& bridge);

    bn::optional<scene_kind> update() override;

private:
    void redraw();

    pksave_bridge&            bridge_;
    bn::sprite_text_generator text_generator_;
    bn::vector<bn::sprite_ptr, 64> text_sprites_;
    int  cursor_;     // 0..4
    bool was_dirty_;  // remembered to detect dirty-flag changes for redraw
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_MAIN_MENU_SCENE_HPP
