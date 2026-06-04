// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Halt scene for unrecoverable boot errors (UNKNOWN save format,
// pksave_open returned NULL / CORRUPT).  Ignores all input and never
// returns a new scene; the user must power-cycle after fixing the
// underlying problem (usually a bad symlink).

#ifndef PKSAVE_GBA_BOOT_ERROR_SCENE_HPP
#define PKSAVE_GBA_BOOT_ERROR_SCENE_HPP

#include "scene.hpp"

#include <bn_sprite_text_generator.h>
#include <bn_string_view.h>
#include <bn_vector.h>
#include <bn_sprite_ptr.h>

namespace pksave_gba {

class boot_error_scene final : public scene {
public:
    // `reason` must outlive the scene (typically a string literal).
    explicit boot_error_scene(const char* reason);

    bn::optional<scene_kind> update() override;

private:
    bn::sprite_text_generator text_generator_;
    bn::vector<bn::sprite_ptr, 32> text_sprites_;
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_BOOT_ERROR_SCENE_HPP
