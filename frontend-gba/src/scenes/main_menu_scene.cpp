// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "main_menu_scene.hpp"

#include "common_variable_8x16_sprite_font.h"

#include <bn_keypad.h>
#include <bn_string.h>

namespace pksave_gba {
namespace {

constexpr int ENTRY_COUNT = 5;
constexpr const char* const ENTRY_LABELS[ENTRY_COUNT] = {
    "Trainer",
    "Party",
    "Bag",
    "Boxes",
    "Save & Exit",
};

// Map menu index -> target scene_kind.  Checkpoint #1 doesn't actually
// transition because none of these scenes exist yet — but the mapping
// is the load-bearing piece for Checkpoint #2 onward, so it lives here
// rather than getting hand-rolled in each subsequent CP.
constexpr scene_kind ENTRY_TARGETS[ENTRY_COUNT] = {
    scene_kind::TRAINER,
    scene_kind::PARTY,
    scene_kind::BAG,
    scene_kind::BOXES,
    scene_kind::SAVE,
};

}  // namespace

main_menu_scene::main_menu_scene(pksave_bridge& bridge)
    : bridge_(bridge)
    , text_generator_(common::variable_8x16_sprite_font)
    , cursor_(0)
    , was_dirty_(bridge.is_dirty())
{
    redraw();
}

void main_menu_scene::redraw()
{
    text_sprites_.clear();

    // Header: game name + dirty indicator.
    bn::string<32> header;
    header.append(game_display_name(bridge_.game()));
    if (bridge_.is_dirty())
    {
        header.append(" *");
    }
    text_generator_.set_center_alignment();
    text_generator_.generate(0, -64, header, text_sprites_);

    // Entries.  Cursor is a leading "> " on the selected row.
    text_generator_.set_left_alignment();
    constexpr int x_left = -56;
    constexpr int y_top  = -24;
    constexpr int row_h  = 16;

    for (int i = 0; i < ENTRY_COUNT; ++i)
    {
        bn::string<32> row;
        row.append((i == cursor_) ? "> " : "  ");
        row.append(ENTRY_LABELS[i]);
        text_generator_.generate(x_left, y_top + i * row_h, row, text_sprites_);
    }
}

bn::optional<scene_kind> main_menu_scene::update()
{
    bool need_redraw = false;

    if (bn::keypad::up_pressed())
    {
        cursor_ = (cursor_ + ENTRY_COUNT - 1) % ENTRY_COUNT;
        need_redraw = true;
    }
    if (bn::keypad::down_pressed())
    {
        cursor_ = (cursor_ + 1) % ENTRY_COUNT;
        need_redraw = true;
    }

    if (bn::keypad::a_pressed())
    {
        const scene_kind target = ENTRY_TARGETS[cursor_];
        // Checkpoint #2: Trainer and Save & Exit are wired.  Party /
        // Bag / Boxes still no-op until Checkpoint #3+.
        switch (target)
        {
            case scene_kind::TRAINER:
            case scene_kind::SAVE:
                return target;
            default:
                // No-op for unimplemented entries.
                break;
        }
    }

    // L/R reserved for top-level page cycling once other scenes land.
    // No-op until then per the plan.

    // Redraw if dirty-flag transitioned (so the "*" appears/disappears
    // mid-session without a full scene swap).
    if (bridge_.is_dirty() != was_dirty_)
    {
        was_dirty_ = bridge_.is_dirty();
        need_redraw = true;
    }

    if (need_redraw)
    {
        redraw();
    }

    return bn::nullopt;
}

}  // namespace pksave_gba
