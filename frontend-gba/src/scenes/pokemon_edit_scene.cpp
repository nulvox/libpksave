// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "pokemon_edit_scene.hpp"
#include "widgets/text_input.hpp"

#include "common_variable_8x16_sprite_font.h"

#include <bn_keypad.h>
#include <bn_log.h>
#include <bn_format.h>

#include <cstring>

namespace pksave_gba {
namespace {

constexpr int ROW_SPECIES  = 0;
constexpr int ROW_NICKNAME = 1;
constexpr int ROW_OT_NAME  = 2;
constexpr int ROW_ITEM     = 3;
constexpr int ROW_ABILITY  = 4;
constexpr int ROW_NATURE   = 5;
constexpr int ROW_SHINY    = 6;
constexpr int IDENTITY_ROW_COUNT = 7;
constexpr int VIEWPORT_ROWS      = 5;

// Per AGENT_PROMPT: Gen 3 dex max is 386 but the species ID space goes
// up to ~411.  pksave_species_name returns "???" for unknown IDs.
constexpr int SPECIES_MIN = 1;
constexpr int SPECIES_MAX = 411;

// Gen 3 item id space per Wave 2A spec.
constexpr int ITEM_MIN = 0;
constexpr int ITEM_MAX = 376;

constexpr int ABILITY_MIN = 0;
constexpr int ABILITY_MAX = 77;

constexpr int NATURE_MIN = 0;
constexpr int NATURE_MAX = 24;

}  // namespace

pokemon_edit_scene::pokemon_edit_scene(pksave_bridge& bridge)
    : bridge_(bridge)
    , text_gen_(common::variable_8x16_sprite_font)
    , list_(text_gen_, IDENTITY_ROW_COUNT, VIEWPORT_ROWS,
            /*x=*/-104, /*y_top=*/-40, /*row_h=*/16)
    , species_(SPECIES_MIN, SPECIES_MIN, SPECIES_MAX)
    , item_(ITEM_MIN, ITEM_MIN, ITEM_MAX)
    , ability_(ABILITY_MIN, ABILITY_MIN, ABILITY_MAX)
    , nature_(NATURE_MIN, NATURE_MIN, NATURE_MAX)
    , pokemon_{}
    , slot_(bridge.pending_party_slot())
    , tab_(edit_tab::IDENTITY)
    , mode_(edit_mode::NAV)
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
    item_.set_id(pokemon_.held_item);
    nature_.set_id(pokemon_.nature);
    ability_.set_filter(&pokemon_edit_scene::ability_filter_thunk, this);
    ability_.set_id(pokemon_.ability);
    consume_text_input_result();

    species_.on_focus_acquired();
    list_.set_row_provider(&pokemon_edit_scene::row_provider_thunk, this);
    redraw_header();
    redraw_right_panel();

    // Scene-entry breadcrumbs for the mGBA-Lua harness.
    BN_LOG("edit: OK");
    BN_LOG("edit: tab=", static_cast<int>(tab_));
}

int pokemon_edit_scene::row_count_for_tab() const
{
    switch (tab_)
    {
        case edit_tab::IDENTITY: return IDENTITY_ROW_COUNT;
        case edit_tab::STATS:    return 0;
        case edit_tab::MOVES:    return 0;
        default:                 return 0;
    }
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
    // Active tab gets uppercase + angle brackets; inactive tabs dim (lower).
    tabs.append(tab_ == edit_tab::IDENTITY ? "<IDENTITY>"  : " identity ");
    tabs.append(" ");
    tabs.append(tab_ == edit_tab::STATS    ? "<STATS>"     : " stats ");
    tabs.append(" ");
    tabs.append(tab_ == edit_tab::MOVES    ? "<MOVES>"     : " moves ");
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

void pokemon_edit_scene::rebuild_for_current_tab()
{
    list_.set_row_count(row_count_for_tab());

    placeholder_sprites_.clear();
    if (tab_ != edit_tab::IDENTITY)
    {
        text_gen_.set_left_alignment();
        const char* msg = (tab_ == edit_tab::STATS)
            ? "TODO Wave 2B: stats"
            : "TODO Wave 2C: moves";
        text_gen_.generate(-104, -40, msg, placeholder_sprites_);
    }
}

void pokemon_edit_scene::row_provider_thunk(void* ctx, int row,
                                            bn::string<32>& out)
{
    static_cast<pokemon_edit_scene*>(ctx)->format_row(row, out);
}

void pokemon_edit_scene::format_row(int row, bn::string<32>& out)
{
    if (tab_ != edit_tab::IDENTITY) return;

    const bool active_here = (mode_ == edit_mode::ACTIVE) &&
                             (list_.selected() == row);
    const char* lb = active_here ? "<" : " ";
    const char* rb = active_here ? ">" : " ";

    switch (row)
    {
        case ROW_SPECIES:
            out.append(bn::format<28>(
                "Species:{}{}{}",
                lb,
                ::pksave_species_name(bridge_.game(), pokemon_.species),
                rb));
            break;
        case ROW_NICKNAME:
        {
            out.append("Nick : ");
            for (int i = 0;
                 i < PKSAVE_GEN3_NICKNAME_MAX && pokemon_.nickname[i] != '\0';
                 ++i)
            {
                out.append(pokemon_.nickname[i]);
            }
            break;
        }
        case ROW_OT_NAME:
        {
            out.append("OT   : ");
            for (int i = 0;
                 i < PKSAVE_GEN3_OT_NAME_MAX && pokemon_.ot_name[i] != '\0';
                 ++i)
            {
                out.append(pokemon_.ot_name[i]);
            }
            break;
        }
        case ROW_ITEM:
            out.append(bn::format<28>(
                "Item :{}{}{}",
                lb,
                ::pksave_item_name(bridge_.game(), pokemon_.held_item),
                rb));
            break;
        case ROW_ABILITY:
            out.append(bn::format<28>(
                "Abil :{}{}{}",
                lb,
                ::pksave_ability_name(bridge_.game(), pokemon_.ability),
                rb));
            break;
        case ROW_NATURE:
            out.append(bn::format<28>(
                "Nat  :{}{}{}",
                lb,
                ::pksave_nature_name(pokemon_.nature),
                rb));
            break;
        case ROW_SHINY:
            out.append(bn::format<24>("Shiny: {}",
                                      pokemon_.is_shiny ? "YES" : "no"));
            break;
        default: break;
    }
}

bool pokemon_edit_scene::ability_filter_thunk(void* ctx, int id)
{
    auto* self = static_cast<pokemon_edit_scene*>(ctx);
    pksave_species_abilities_t ab =
        ::pksave_species_abilities(self->bridge_.game(),
                                   self->pokemon_.species);
    // Slot 1 ability id 0 means "this species has no second ability".
    if (id == ab.slot0 && ab.slot0 != 0) return true;
    if (id == ab.slot1 && ab.slot1 != 0) return true;
    return false;
}

void pokemon_edit_scene::consume_text_input_result()
{
    auto& mbx = text_input_state();
    if (!mbx.result.valid) return;
    mbx.result.valid = false;

    const auto target = static_cast<edit_text_target>(mbx.result.caller_tag);

    if (mbx.result.cancelled) return;
    if (target != edit_text_target::NICKNAME &&
        target != edit_text_target::OT_NAME) return;

    char* dst = (target == edit_text_target::NICKNAME)
        ? pokemon_.nickname
        : pokemon_.ot_name;
    const int max_len = (target == edit_text_target::NICKNAME)
        ? PKSAVE_GEN3_NICKNAME_MAX
        : PKSAVE_GEN3_OT_NAME_MAX;

    std::memset(dst, 0, 13);
    const int len = bn::min<int>(mbx.result.value.size(), max_len);
    for (int i = 0; i < len; ++i)
    {
        dst[i] = mbx.result.value[i];
    }
    push_to_save();
}

void pokemon_edit_scene::open_text_input_for(edit_text_target target)
{
    auto& mbx = text_input_state();
    mbx.request.initial.clear();

    const char* src = (target == edit_text_target::NICKNAME)
        ? pokemon_.nickname
        : pokemon_.ot_name;
    const int max_len = (target == edit_text_target::NICKNAME)
        ? PKSAVE_GEN3_NICKNAME_MAX
        : PKSAVE_GEN3_OT_NAME_MAX;

    for (int i = 0; i < max_len && src[i] != '\0'; ++i)
    {
        mbx.request.initial.append(src[i]);
    }
    mbx.request.max_len    = max_len;
    mbx.request.return_to  = scene_kind::POKEMON_EDIT;
    mbx.request.caller_tag = static_cast<int>(target);
    mbx.result.valid       = false;
}

void pokemon_edit_scene::enter_active()
{
    mode_ = edit_mode::ACTIVE;
    const int sel = list_.selected();
    switch (sel)
    {
        case ROW_SPECIES: species_.on_focus_acquired(); break;
        case ROW_ITEM:    item_.on_focus_acquired();    break;
        case ROW_ABILITY: ability_.on_focus_acquired(); break;
        case ROW_NATURE:  nature_.on_focus_acquired();  break;
        default: break;
    }
    BN_LOG("edit: mode=ACTIVE row=", sel);
}

void pokemon_edit_scene::exit_active()
{
    mode_ = edit_mode::NAV;
    BN_LOG("edit: mode=NAV");
}

bn::optional<scene_kind> pokemon_edit_scene::update()
{
    bool need_header_redraw = false;
    bool need_list_redraw   = false;
    bool need_right_redraw  = false;

    // B handling: in ACTIVE, exit ACTIVE (no revert); in NAV, return PARTY.
    if (bn::keypad::b_pressed())
    {
        if (mode_ == edit_mode::ACTIVE)
        {
            exit_active();
            list_.redraw();
            return bn::nullopt;
        }
        return scene_kind::PARTY;
    }

    // D-pad LEFT/RIGHT in NAV mode: cycle tabs.
    if (mode_ == edit_mode::NAV)
    {
        bool tab_changed = false;
        if (bn::keypad::left_pressed())
        {
            tab_ = static_cast<edit_tab>(
                (static_cast<int>(tab_) + 2) % 3);
            tab_changed = true;
        }
        else if (bn::keypad::right_pressed())
        {
            tab_ = static_cast<edit_tab>(
                (static_cast<int>(tab_) + 1) % 3);
            tab_changed = true;
        }
        if (tab_changed)
        {
            BN_LOG("edit: tab=", static_cast<int>(tab_));
            rebuild_for_current_tab();
            redraw_header();
            return bn::nullopt;
        }
    }

    // List navigation (UP/DOWN) suppressed when ACTIVE so the user
    // doesn't slide off the row being edited.
    if (mode_ == edit_mode::NAV && tab_ == edit_tab::IDENTITY)
    {
        const bool selection_changed = list_.tick();
        const int sel = list_.selected();

        if (selection_changed)
        {
            switch (sel)
            {
                case ROW_SPECIES: species_.on_focus_acquired(); break;
                case ROW_ITEM:    item_.on_focus_acquired();    break;
                case ROW_ABILITY: ability_.on_focus_acquired(); break;
                case ROW_NATURE:  nature_.on_focus_acquired();  break;
                default: break;
            }
            need_list_redraw = true;
        }
    }

    const int sel = list_.selected();

    // A handling depends on mode and row.
    if (bn::keypad::a_pressed() && tab_ == edit_tab::IDENTITY)
    {
        if (mode_ == edit_mode::NAV)
        {
            switch (sel)
            {
                case ROW_NICKNAME:
                    open_text_input_for(edit_text_target::NICKNAME);
                    return scene_kind::TEXT_INPUT;
                case ROW_OT_NAME:
                    open_text_input_for(edit_text_target::OT_NAME);
                    return scene_kind::TEXT_INPUT;
                case ROW_SHINY:
                    pokemon_.is_shiny = pokemon_.is_shiny ? 0 : 1;
                    push_to_save();
                    need_list_redraw   = true;
                    need_header_redraw = true;
                    break;
                case ROW_SPECIES:
                case ROW_ITEM:
                case ROW_ABILITY:
                case ROW_NATURE:
                    enter_active();
                    need_list_redraw = true;
                    break;
                default: break;
            }
        }
        else // ACTIVE: A commits
        {
            exit_active();
            need_list_redraw = true;
        }
    }

    // Per-row cycler input only when in ACTIVE on that row.
    if (mode_ == edit_mode::ACTIVE && tab_ == edit_tab::IDENTITY)
    {
        switch (sel)
        {
            case ROW_SPECIES:
                if (species_.tick())
                {
                    BN_LOG("edit: cycle sp=", species_.id());
                    pokemon_.species = static_cast<uint16_t>(species_.id());
                    // Species change can invalidate ability slot — reseed
                    // to slot0 (which always exists for real species).
                    pksave_species_abilities_t ab =
                        ::pksave_species_abilities(bridge_.game(),
                                                   pokemon_.species);
                    if (ab.slot0 != 0)
                    {
                        pokemon_.ability = ab.slot0;
                        ability_.set_id(ab.slot0);
                    }
                    push_to_save();
                    need_list_redraw   = true;
                    need_right_redraw  = true;
                    need_header_redraw = true;
                }
                break;
            case ROW_ITEM:
                if (item_.tick())
                {
                    pokemon_.held_item = static_cast<uint16_t>(item_.id());
                    push_to_save();
                    need_list_redraw   = true;
                    need_header_redraw = true;
                }
                break;
            case ROW_ABILITY:
                if (ability_.tick())
                {
                    pokemon_.ability = static_cast<uint8_t>(ability_.id());
                    push_to_save();
                    need_list_redraw   = true;
                    need_header_redraw = true;
                }
                break;
            case ROW_NATURE:
                if (nature_.tick())
                {
                    pokemon_.nature = static_cast<uint8_t>(nature_.id());
                    push_to_save();
                    need_list_redraw   = true;
                    need_header_redraw = true;
                }
                break;
            default: break;
        }
    }

    if (need_list_redraw)   list_.redraw();
    if (need_right_redraw)  redraw_right_panel();
    if (need_header_redraw) redraw_header();

    return bn::nullopt;
}

}  // namespace pksave_gba
