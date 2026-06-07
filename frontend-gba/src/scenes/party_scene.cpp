// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "party_scene.hpp"

#include "common_variable_8x16_sprite_font.h"

#include <bn_keypad.h>
#include <bn_log.h>
#include <bn_format.h>

#include <cstring>

namespace pksave_gba {
namespace {

constexpr int ROW_COUNT      = 6;   // Gen III party is always 6 slots.
constexpr int VIEWPORT_ROWS  = 6;

}  // namespace

party_scene::party_scene(pksave_bridge& bridge)
    : bridge_(bridge)
    , text_gen_(common::variable_8x16_sprite_font)
    , list_(text_gen_, ROW_COUNT, VIEWPORT_ROWS,
            /*x=*/-104, /*y_top=*/-40, /*row_h=*/16)
    , party_{}
    , party_count_(0)
{
    load_from_save();
    list_.set_row_provider(&party_scene::row_provider_thunk, this);
    redraw_header();
    // Scene-entry breadcrumb for the mGBA-Lua harness.  The test image's
    // logger picks up the gba.debug category at mLOG_ALL (see
    // docker/test-gba/mgba-lua.c); test_party_scene_nav.py greps stdout
    // for "party: OK".
    BN_LOG("party: OK");
}

void party_scene::load_from_save()
{
    if (!bridge_.ok())
    {
        return;
    }
    std::memset(party_, 0, sizeof(party_));
    party_count_ = 0;
    ::pksave_get_party(bridge_.handle(), party_, &party_count_);
}

void party_scene::redraw_header()
{
    header_sprites_.clear();
    text_gen_.set_center_alignment();

    bn::string<32> header;
    header.append("PARTY");
    if (bridge_.is_dirty())
    {
        header.append(" *");
    }
    text_gen_.generate(0, -72, header, header_sprites_);
}

void party_scene::row_provider_thunk(void* ctx, int row, bn::string<32>& out)
{
    static_cast<party_scene*>(ctx)->format_row(row, out);
}

void party_scene::format_row(int row, bn::string<32>& out)
{
    // A slot is "empty" if its index is beyond party_count_ OR its
    // species id is zero (defensive: libpksave should return contiguous
    // filled-then-empty, but the renderer treats species==0 as empty
    // regardless of count to match how the games display empty slots).
    const bool empty = (row >= party_count_) ||
                       (party_[row].species == 0);
    if (empty)
    {
        out.append("[Empty]");
        return;
    }

    const pksave_pokemon_t& mon = party_[row];
    const char* species = ::pksave_species_name(bridge_.game(), mon.species);
    out.append(bn::format<24>("{} L{} ", species, mon.level));
    for (int i = 0;
         i < PKSAVE_GEN3_NICKNAME_MAX && mon.nickname[i] != '\0';
         ++i)
    {
        out.append(mon.nickname[i]);
    }
}

bn::optional<scene_kind> party_scene::update()
{
    if (bn::keypad::b_pressed())
    {
        return scene_kind::MAIN_MENU;
    }

    // list_view handles its own redraw on selection change; no
    // per-row widgets to disarm (cf. trainer_scene's numeric_field
    // disarm pattern), so the return value is unused.
    list_.tick();

    if (bn::keypad::a_pressed())
    {
        bridge_.set_pending_party_slot(list_.selected());
        // CP#3-C wires this to scene_kind::POKEMON_EDIT.
        // For CP#3-B the drill is a no-op transition (stay on PARTY).
        return bn::nullopt;
    }

    return bn::nullopt;
}

}  // namespace pksave_gba
