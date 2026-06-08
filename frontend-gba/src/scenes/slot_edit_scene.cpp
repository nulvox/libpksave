// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "slot_edit_scene.hpp"

#include "common_variable_8x16_sprite_font.h"

#include <bn_keypad.h>
#include <bn_log.h>
#include <bn_format.h>

#include <cstring>

namespace pksave_gba {
namespace {

constexpr int ROW_ITEM = 0;
constexpr int ROW_QTY  = 1;
constexpr int ROW_COUNT = 2;

constexpr int ITEM_MIN = 0;     // 0 = (none) / empty slot
constexpr int ITEM_MAX = 376;   // matches Gen 3 item id space

constexpr int QTY_MIN  = 0;
// QTY_MAX is reseated from pksave_item_max_stack on every item change.
// Seed wide so the first set_max() can clamp down.
constexpr int QTY_CAP_INITIAL = 999;

}  // namespace

slot_edit_scene::slot_edit_scene(pksave_bridge& bridge)
    : bridge_(bridge)
    , text_gen_(common::variable_8x16_sprite_font)
    , list_(text_gen_, ROW_COUNT, /*viewport_rows=*/ROW_COUNT,
            /*x=*/-104, /*y_top=*/-24, /*row_h=*/16)
    , item_(/*initial_id=*/ITEM_MIN, ITEM_MIN, ITEM_MAX)
    , qty_(/*initial=*/0, QTY_MIN, QTY_CAP_INITIAL)
    , pocket_(bridge.pending_bag_pocket())
    , slot_(bridge.pending_bag_slot())
{
    load_slot();
    // Cycler filter restricts steps to items whose pksave_item_pocket
    // matches the handoff pocket — except item_id 0 (none), which is
    // always valid as an empty-slot sentinel so the user can clear.
    item_.set_filter(&slot_edit_scene::pocket_filter_thunk, this);
    resync_qty_max();
    list_.set_row_provider(&slot_edit_scene::row_provider_thunk, this);
    redraw_header();
    BN_LOG("slot_edit: OK");
}

void slot_edit_scene::load_slot()
{
    // Snapshot the current pocket and read out slot_'s item.
    pksave_item_t pocket_items[64] = {};
    uint8_t count = 0;
    if (bridge_.ok())
    {
        ::pksave_get_pocket(bridge_.handle(), pocket_, pocket_items, &count);
    }
    if (slot_ >= 0 && slot_ < count)
    {
        item_.set_id(pocket_items[slot_].item_id);
        qty_.set_value(pocket_items[slot_].quantity);
    }
    else
    {
        // Slot beyond the current count = new "empty" slot.
        item_.set_id(ITEM_MIN);
        qty_.set_value(0);
    }
}

void slot_edit_scene::resync_qty_max()
{
    const uint16_t cap =
        ::pksave_item_max_stack(bridge_.game(), item_.id());
    // Cap of 0 happens for item_id 0 (empty) and for unknown IDs.  Allow
    // the qty field to be 0 in those cases by setting max=0, which clamps
    // qty into [0, 0].  When the user cycles to a real item, max jumps
    // back up.
    qty_.set_max(static_cast<int>(cap));
}

void slot_edit_scene::redraw_header()
{
    header_sprites_.clear();
    text_gen_.set_center_alignment();
    bn::string<32> header;
    header.append("EDIT SLOT ");
    header.append(bn::format<6>("{}", slot_));
    if (bridge_.is_dirty())
    {
        header.append(" *");
    }
    text_gen_.generate(0, -72, header, header_sprites_);
}

void slot_edit_scene::row_provider_thunk(void* ctx, int row,
                                         bn::string<32>& out)
{
    static_cast<slot_edit_scene*>(ctx)->format_row(row, out);
}

void slot_edit_scene::format_row(int row, bn::string<32>& out)
{
    if (row == ROW_ITEM)
    {
        const char* name = (item_.id() == 0)
            ? "(none)"
            : ::pksave_item_name(bridge_.game(), item_.id());
        out.append(bn::format<28>("Item: <{}>", name));
    }
    else
    {
        out.append(bn::format<28>("Qty:  {} / {}", qty_.value(), qty_.max()));
    }
}

bool slot_edit_scene::pocket_filter_thunk(void* ctx, int id)
{
    auto* self = static_cast<slot_edit_scene*>(ctx);
    if (id == 0)
    {
        // Item 0 (none) is always reachable: cycling to it deletes the
        // slot on commit (qty forced to 0).
        return true;
    }
    return ::pksave_item_pocket(self->bridge_.game(),
                                static_cast<uint16_t>(id)) == self->pocket_;
}

bn::optional<scene_kind> slot_edit_scene::update()
{
    if (bn::keypad::b_pressed())
    {
        bridge_.clear_pending_bag();
        return scene_kind::BAG;
    }

    const bool selection_changed = list_.tick();
    const int sel = list_.selected();

    // Disarm on focus transfer: the navigation edge that just landed on a
    // level-triggered widget would otherwise be re-interpreted as a
    // frame-1 step.  Same trap as trainer_scene's numeric_field
    // (docs/frontend-gba-plan.md §"Pitfall — list_view consumes UP/DOWN
    // edges").
    if (selection_changed)
    {
        if (sel == ROW_ITEM) { item_.on_focus_acquired(); }
        if (sel == ROW_QTY)  { qty_.on_focus_acquired();  }
    }

    if (sel == ROW_ITEM)
    {
        if (item_.tick())
        {
            resync_qty_max();
            list_.redraw();
        }
    }
    else
    {
        if (qty_.tick())
        {
            list_.redraw();
        }
    }

    if (bn::keypad::a_pressed())
    {
        pksave_item_t out;
        // If the user committed with item_id 0, force qty 0 so the row
        // representing an empty slot is well-formed.
        out.item_id  = static_cast<uint16_t>(item_.id());
        out.quantity = (out.item_id == 0) ? 0
                                          : static_cast<uint16_t>(qty_.value());

        if (bridge_.ok() && slot_ >= 0)
        {
            const int rc = ::pksave_set_item(bridge_.handle(),
                                             pocket_,
                                             static_cast<uint8_t>(slot_),
                                             &out);
            if (rc == PKSAVE_OK)
            {
                bridge_.mark_dirty();
            }
        }
        bridge_.clear_pending_bag();
        return scene_kind::BAG;
    }
    return bn::nullopt;
}

}  // namespace pksave_gba
