// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Scene base class + transition enum.
//
// Each concrete scene owns its own butano resources (sprites, BGs, text
// items) by value; constructing a scene creates resources, destroying
// it releases them.  The main loop holds the current scene in
// std::unique_ptr<scene>, and swaps it when update() returns a new
// scene_kind — destroying the old scene first frees its VRAM before
// the new one is constructed.
//
// scene_kind enumerates every reachable scene, including future ones.
// Listing them here (rather than waiting until each lands) is required
// for main_menu_scene to map menu selections to transition values; it
// is not speculative.

#ifndef PKSAVE_GBA_SCENE_HPP
#define PKSAVE_GBA_SCENE_HPP

#include <bn_optional.h>

namespace pksave_gba {

enum class scene_kind {
    BOOT_ERROR,
    MAIN_MENU,
    TRAINER,
    PARTY,
    BAG,
    BOXES,
    SAVE,
    TEXT_INPUT,
};

class scene {
public:
    virtual ~scene() = default;

    // Called once per frame.  Read butano input here, mutate scene
    // state, draw nothing here that would survive across the frame
    // boundary (sprite_ptr etc. are member-owned).  Return:
    //   - bn::nullopt to stay on this scene
    //   - some(kind) to request a transition to scene `kind`
    virtual bn::optional<scene_kind> update() = 0;
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_SCENE_HPP
