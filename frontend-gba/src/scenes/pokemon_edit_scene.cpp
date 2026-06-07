// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "pokemon_edit_scene.hpp"

#include "common_variable_8x16_sprite_font.h"

#include <bn_keypad.h>
#include <bn_log.h>
#include <bn_format.h>

#include <cstring>

namespace pksave_gba {
namespace {

constexpr int ROW_SPECIES = 0;
constexpr int ROW_COUNT   = 1;
constexpr int VIEWPORT_ROWS = 1;

// Per AGENT_PROMPT: Gen 3 dex max is 386 but the species ID space goes
// up to ~411.  Pick [1, 411] as the cycler range; pksave_species_name
// returns "???" for unknown IDs.  Wave 2 can filter if needed.
constexpr int SPECIES_MIN = 1;
constexpr int SPECIES_MAX = 411;

}  // namespace

pokemon_edit_scene::pokemon_edit_scene(pksave_bridge& bridge)
    : bridge_(bridge)
    , text_gen_(common::variable_8x16_sprite_font)
    , list_(text_gen_, ROW_COUNT, VIEWPORT_ROWS,
            /*x=*/-104, /*y_top=*/-40, /*row_h=*/16)
    , species_(SPECIES_MIN, SPECIES_MIN, SPECIES_MAX)
    , pokemon_{}
    , slot_(bridge.pending_party_slot())
{
    // Defensive: party_scene is the only caller and always sets the
    // slot before transitioning.  If the slot is unset (-1) we fall
    // back to slot 0 so the scene still constructs cleanly.
    if (slot_ < 0 || slot_ >= 6)
    {
        slot_ = 0;
    }

    load_from_save();
    species_.set_id(pokemon_.species);
    species_.on_focus_acquired();

    list_.set_row_provider(&pokemon_edit_scene::row_provider_thunk, this);
    redraw_header();
    redraw_right_panel();

    // Scene-entry breadcrumb for the mGBA-Lua harness.
    BN_LOG("edit: OK");
}

void pokemon_edit_scene::load_from_save()
{
    if (!bridge_.ok())
    {
        return;
    }
    pksave_pokemon_t party[6] = {};
    uint8_t count = 0;
    ::pksave_get_party(bridge_.handle(), party, &count);
    pokemon_ = party[slot_];
}

void pokemon_edit_scene::push_to_save()
{
    if (!bridge_.ok())
    {
        return;
    }
    int rc = ::pksave_set_party_slot(bridge_.handle(),
                                     static_cast<uint8_t>(slot_),
                                     &pokemon_);
    BN_LOG("edit: push slot=", slot_, " sp=", pokemon_.species, " rc=", rc);
    bridge_.mark_dirty();

    // Verify by reading back
    pksave_pokemon_t verify[6] = {};
    uint8_t cnt = 0;
    ::pksave_get_party(bridge_.handle(), verify, &cnt);
    BN_LOG("edit: verify sp=", verify[slot_].species, " cnt=", cnt);
}

void pokemon_edit_scene::redraw_header()
{
    header_sprites_.clear();
    text_gen_.set_center_alignment();

    bn::string<32> tabs;
    // IDENTITY active (uppercase), the other two dim (lowercase) until
    // wave 2 wires tab cycling.  TODO(wave-2): D-pad left/right cycles
    // tabs when no field is in active-edit mode (plan §9).
    tabs.append("IDENTITY  stats  moves");
    if (bridge_.is_dirty())
    {
        tabs.append(" *");
    }
    text_gen_.generate(0, -72, tabs, header_sprites_);
}

void pokemon_edit_scene::redraw_right_panel()
{
    right_panel_sprites_.clear();
    text_gen_.set_left_alignment();

    constexpr int x_right = 32;
    constexpr int y_top   = -32;
    constexpr int row_h   = 16;

    bn::string<24> silhouette;
    silhouette.append("[?]");
    text_gen_.generate(x_right, y_top + 0 * row_h, silhouette,
                       right_panel_sprites_);

    bn::string<24> species_line;
    species_line.append(::pksave_species_name(bridge_.game(),
                                              pokemon_.species));
    text_gen_.generate(x_right, y_top + 1 * row_h, species_line,
                       right_panel_sprites_);

    text_gen_.generate(x_right, y_top + 2 * row_h,
                       bn::format<8>("L{}", pokemon_.level),
                       right_panel_sprites_);
}

void pokemon_edit_scene::row_provider_thunk(void* ctx, int row,
                                            bn::string<32>& out)
{
    static_cast<pokemon_edit_scene*>(ctx)->format_row(row, out);
}

void pokemon_edit_scene::format_row(int row, bn::string<32>& out)
{
    if (row == ROW_SPECIES)
    {
        out.append(bn::format<28>(
            "Species: <{}>",
            ::pksave_species_name(bridge_.game(), pokemon_.species)));
    }
}

bn::optional<scene_kind> pokemon_edit_scene::update()
{
    if (bn::keypad::b_pressed())
    {
        return scene_kind::PARTY;
    }

    bool need_header_redraw = false;
    bool need_list_redraw   = false;
    bool need_right_redraw  = false;

    const bool selection_changed = list_.tick();
    const int sel = list_.selected();

    if (selection_changed)
    {
        // ROW_SPECIES is the only row in this slice; defensively disarm
        // the cycler on selection change so a still-held LEFT/RIGHT from
        // navigation can't be re-read as a phantom edge (pattern from
        // trainer_scene + numeric_field).
        if (sel == ROW_SPECIES) species_.on_focus_acquired();
        need_list_redraw = true;
    }

    if (sel == ROW_SPECIES)
    {
        if (species_.tick())
        {
            BN_LOG("edit: cycle ", species_.id());
            pokemon_.species = static_cast<uint16_t>(species_.id());
            push_to_save();
            need_list_redraw   = true;
            need_right_redraw  = true;
            need_header_redraw = true;  // dirty flag may have flipped
        }
    }

    if (need_list_redraw)   list_.redraw();
    if (need_right_redraw)  redraw_right_panel();
    if (need_header_redraw) redraw_header();

    return bn::nullopt;
}

}  // namespace pksave_gba
