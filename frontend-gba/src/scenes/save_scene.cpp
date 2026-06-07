// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "save_scene.hpp"

#include "common_variable_8x16_sprite_font.h"

#include <bn_log.h>

namespace pksave_gba {

save_scene::save_scene(pksave_bridge& bridge)
    : bridge_(bridge)
    , text_gen_(common::variable_8x16_sprite_font)
    , phase_(0)
    , flush_ok_(false)
{
    text_gen_.set_center_alignment();
    text_gen_.generate(0, -16, "SAVING...",                       sprites_);
    text_gen_.generate(0,   8, "DON'T TURN OFF THE POWER!",       sprites_);
}

bn::optional<scene_kind> save_scene::update()
{
    // Phase 0: let the first display frame go by so the user actually
    // sees "SAVING..." before the synchronous FLASH write hangs the
    // scene loop.  Phase advances unconditionally — input is ignored.
    if (phase_ == 0)
    {
        phase_ = 1;
        return bn::nullopt;
    }

    if (phase_ == 1)
    {
        BN_LOG("save: flush start");
        flush_ok_ = bridge_.flush_to_flash();
        BN_LOG("save: flush done ok=", flush_ok_);
        phase_ = flush_ok_ ? 3 : 2;
        if (!flush_ok_)
        {
            // Replace the message with an error.  Sprites already in
            // flight get replaced wholesale on next clear.
            sprites_.clear();
            text_gen_.set_center_alignment();
            text_gen_.generate(0, -16, "SAVE FAILED.", sprites_);
            text_gen_.generate(0,   8, "Power off and retry.", sprites_);
        }
        return bn::nullopt;
    }

    if (phase_ == 3)
    {
        // Success — head back to main menu so the user sees the
        // dirty indicator cleared.
        return scene_kind::MAIN_MENU;
    }

    // phase_ == 2: error halt.  Stay forever.
    return bn::nullopt;
}

}  // namespace pksave_gba
