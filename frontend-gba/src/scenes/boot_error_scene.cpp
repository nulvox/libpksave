// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "boot_error_scene.hpp"

#include "common_variable_8x16_sprite_font.h"

namespace pksave_gba {

boot_error_scene::boot_error_scene(const char* reason)
    : text_generator_(common::variable_8x16_sprite_font)
{
    text_generator_.set_center_alignment();
    text_generator_.generate(0, -32, "PKSAVE — BOOT ERROR", text_sprites_);
    text_generator_.generate(0,   0, reason,                text_sprites_);
    text_generator_.generate(0,  32, "Power off and re-check the save symlink.",
                             text_sprites_);
}

bn::optional<scene_kind> boot_error_scene::update()
{
    // Ignore all input.  The screen halts here until the user power-cycles.
    return bn::nullopt;
}

}  // namespace pksave_gba
