// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Blocking "SAVING — DO NOT TURN OFF THE POWER!" screen.
//
// On entry: render the message and ignore input for one frame, then on
// the second update() call flush_to_flash().  The single-frame delay
// matters because flash_write_all is synchronous and can finish faster
// than one display frame — if we flushed on the constructor's draw we
// might never present the message to the user.

#ifndef PKSAVE_GBA_SAVE_SCENE_HPP
#define PKSAVE_GBA_SAVE_SCENE_HPP

#include "scene.hpp"
#include "pksave_bridge.hpp"

#include <bn_sprite_text_generator.h>
#include <bn_sprite_ptr.h>
#include <bn_vector.h>

namespace pksave_gba {

class save_scene final : public scene {
public:
    explicit save_scene(pksave_bridge& bridge);

    bn::optional<scene_kind> update() override;

private:
    pksave_bridge&                bridge_;
    bn::sprite_text_generator     text_gen_;
    bn::vector<bn::sprite_ptr, 32> sprites_;
    // 0 = waiting one display frame so the message is on-screen.
    // 1 = perform the flush, then return to main menu.
    // 2 = error path: flush failed; remain here forever so the user
    //     can power-cycle (same UX as boot_error_scene).
    int  phase_;
    bool flush_ok_;
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_SAVE_SCENE_HPP
