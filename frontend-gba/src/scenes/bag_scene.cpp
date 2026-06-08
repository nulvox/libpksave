// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "bag_scene.hpp"

#include "common_variable_8x16_sprite_font.h"

#include <bn_keypad.h>
#include <bn_log.h>
#include <bn_format.h>

#include <cstring>

namespace pksave_gba {
namespace {

// Tab order, left-to-right.  Matches the order shown in-game and is the
// order users will cycle through with D-pad LEFT/RIGHT.
constexpr pksave_pocket_t POCKET_ORDER[5] = {
    PKSAVE_POCKET_ITEMS,
    PKSAVE_POCKET_BALLS,
    PKSAVE_POCKET_TM_HM,
    PKSAVE_POCKET_KEY,
    PKSAVE_POCKET_BERRIES,
};
constexpr const char* const POCKET_LABEL[5] = {
    "ITEMS", "BALLS", "TM/HM", "KEY", "BERRIES",
};
constexpr int POCKET_COUNT = 5;

// One row per slot.  Empty trailing slots are shown so the user can add
// to the pocket without first having to remember where it ends.
constexpr int VIEWPORT_ROWS = 5;

}  // namespace

bag_scene::bag_scene(pksave_bridge& bridge)
    : bridge_(bridge)
    , text_gen_(common::variable_8x16_sprite_font)
    , items_{}
    , item_count_(0)
    , pocket_index_(0)
    , list_(text_gen_, /*row_count=*/1, VIEWPORT_ROWS,
            /*x=*/-104, /*y_top=*/-24, /*row_h=*/16)
{
    // Clear any stale handoff state so the first frame doesn't think it
    // just returned from slot_edit_scene.
    bridge_.clear_pending_bag();
    load_pocket();
    list_.set_row_provider(&bag_scene::row_provider_thunk, this);
    redraw_header();
    redraw_tabs();
    // Scene-entry breadcrumb for the mGBA-Lua harness.
    BN_LOG("bag: OK");
}

void bag_scene::load_pocket()
{
    std::memset(items_, 0, sizeof(items_));
    item_count_ = 0;
    if (bridge_.ok())
    {
        ::pksave_get_pocket(bridge_.handle(),
                            POCKET_ORDER[pocket_index_],
                            items_, &item_count_);
    }
    // Show at least one row so the list_view isn't pathological; the row
    // provider renders "[Empty]" beyond item_count_.
    const int rows = (item_count_ > 0) ? static_cast<int>(item_count_) + 1
                                        : 1;
    list_.set_row_count(rows);
}

void bag_scene::redraw_header()
{
    header_sprites_.clear();
    text_gen_.set_center_alignment();

    bn::string<32> header;
    header.append("BAG ");
    header.append(POCKET_LABEL[pocket_index_]);
    if (bridge_.is_dirty())
    {
        header.append(" *");
    }
    text_gen_.generate(0, -72, header, header_sprites_);
}

void bag_scene::redraw_tabs()
{
    tab_sprites_.clear();
    text_gen_.set_left_alignment();
    // Tabs above the slot list.  Each label rendered separately so the
    // focused one can be drawn with brackets.
    constexpr int x_left = -104;
    constexpr int y      = -48;
    int x = x_left;
    for (int i = 0; i < POCKET_COUNT; ++i)
    {
        bn::string<10> cell;
        if (i == pocket_index_)
        {
            cell.append("[");
            cell.append(POCKET_LABEL[i]);
            cell.append("]");
        }
        else
        {
            cell.append(" ");
            cell.append(POCKET_LABEL[i]);
            cell.append(" ");
        }
        text_gen_.generate(x, y, cell, tab_sprites_);
        // Each cell takes roughly 9 chars * 6 px = 54 px wide; use 44 to
        // keep the tab row inside the 240 px screen.
        x += 44;
    }
}

void bag_scene::row_provider_thunk(void* ctx, int row, bn::string<32>& out)
{
    static_cast<bag_scene*>(ctx)->format_row(row, out);
}

void bag_scene::format_row(int row, bn::string<32>& out)
{
    if (row >= item_count_)
    {
        out.append("[Empty]");
        return;
    }
    const pksave_item_t& it = items_[row];
    const char* name = ::pksave_item_name(bridge_.game(), it.item_id);
    out.append(bn::format<28>("{} x{}", name, it.quantity));
}

bn::optional<scene_kind> bag_scene::update()
{
    bool need_full_redraw = false;

    if (bn::keypad::b_pressed())
    {
        return scene_kind::MAIN_MENU;
    }

    // D-pad LEFT/RIGHT cycle pocket tabs.  Saturating, no wrap — matches
    // the species/move cycler convention so users have one mental model
    // for "horizontal navigation that doesn't wrap."
    if (bn::keypad::left_pressed() && pocket_index_ > 0)
    {
        --pocket_index_;
        load_pocket();
        need_full_redraw = true;
    }
    if (bn::keypad::right_pressed() && pocket_index_ < POCKET_COUNT - 1)
    {
        ++pocket_index_;
        load_pocket();
        need_full_redraw = true;
    }

    list_.tick();

    if (bn::keypad::a_pressed())
    {
        // Record the handoff for slot_edit_scene.  Even an "empty slot"
        // row is a valid drill target — the user adds a new item by
        // entering the editor with item_id = 0 and qty = 0, then cycling
        // up.  list_view's selected() is the slot index.
        bridge_.set_pending_bag(POCKET_ORDER[pocket_index_], list_.selected());
        return scene_kind::SLOT_EDIT;
    }

    if (need_full_redraw)
    {
        redraw_header();
        redraw_tabs();
    }
    return bn::nullopt;
}

}  // namespace pksave_gba
