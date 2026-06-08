// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "box_scene.hpp"

#include "common_variable_8x16_sprite_font.h"

#include <bn_keypad.h>
#include <bn_log.h>
#include <bn_format.h>

#include <cstring>

namespace pksave_gba {
namespace {

constexpr int VIEWPORT_ROWS = 6;

}  // namespace

box_scene::box_scene(pksave_bridge& bridge)
    : bridge_(bridge)
    , text_gen_(common::variable_8x16_sprite_font)
    , list_(text_gen_, SLOTS_PER_BOX, VIEWPORT_ROWS,
            /*x=*/-104, /*y_top=*/-40, /*row_h=*/16)
    , box_{}
    , box_count_(0)
    , box_index_(0)
    , box_total_(0)
{
    // Idempotent re-entry: drop any stale handoff so the first frame
    // doesn't get reinterpreted as "just returned from pokemon_edit".
    bridge_.clear_pending_box();

    if (bridge_.ok())
    {
        box_total_ = ::pksave_get_box_count(bridge_.handle());
    }
    if (box_total_ <= 0)
    {
        box_total_ = 1;   // defensive: never divide-by-zero on box cycling
    }

    load_box();
    list_.set_row_provider(&box_scene::row_provider_thunk, this);
    redraw_header();

    // Scene-entry breadcrumb for the mGBA-Lua harness.
    BN_LOG("box: OK");
}

void box_scene::load_box()
{
    std::memset(box_, 0, sizeof(box_));
    box_count_ = 0;
    if (bridge_.ok())
    {
        ::pksave_get_box(bridge_.handle(),
                         static_cast<uint8_t>(box_index_),
                         box_, &box_count_);
    }
}

void box_scene::redraw_header()
{
    header_sprites_.clear();
    text_gen_.set_center_alignment();

    bn::string<32> header;
    // 1-indexed for the user (matches the in-game UI).
    header.append(bn::format<24>("BOX {}  {}/{}",
                                 box_index_ + 1,
                                 static_cast<int>(box_count_),
                                 SLOTS_PER_BOX));
    if (bridge_.is_dirty())
    {
        header.append(" *");
    }
    text_gen_.generate(0, -72, header, header_sprites_);
}

void box_scene::row_provider_thunk(void* ctx, int row, bn::string<32>& out)
{
    static_cast<box_scene*>(ctx)->format_row(row, out);
}

void box_scene::format_row(int row, bn::string<32>& out)
{
    // Same "species==0 → empty" rule party_scene uses, since libpksave
    // packs filled slots contiguously starting at index 0 but the slot
    // structs zero-fill beyond box_count_.
    const bool empty = (row >= box_count_) || (box_[row].species == 0);
    if (empty)
    {
        out.append("[Empty]");
        return;
    }

    const pksave_pokemon_t& mon = box_[row];
    const char* species = ::pksave_species_name(bridge_.game(), mon.species);
    out.append(bn::format<24>("{} L{} ", species, mon.level));
    for (int i = 0;
         i < PKSAVE_GEN3_NICKNAME_MAX && mon.nickname[i] != '\0';
         ++i)
    {
        out.append(mon.nickname[i]);
    }
}

bn::optional<scene_kind> box_scene::update()
{
    if (bn::keypad::b_pressed())
    {
        return scene_kind::MAIN_MENU;
    }

    // SELECT cycles boxes (wraps).  Plan §5 reserves SELECT for this.
    if (bn::keypad::select_pressed())
    {
        box_index_ = (box_index_ + 1) % box_total_;
        load_box();
        list_.redraw();
        redraw_header();
    }

    list_.tick();

    if (bn::keypad::a_pressed())
    {
        // Record the handoff for pokemon_edit_scene.  Drilling on an
        // empty slot is intentional — editing the species cycler from
        // 0 to non-zero creates the entry on commit.
        bridge_.set_pending_box(box_index_, list_.selected());
        return scene_kind::POKEMON_EDIT;
    }

    return bn::nullopt;
}

}  // namespace pksave_gba
