// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "pokemon_edit_scene.hpp"
#include "species_sprite_lookup.hpp"
#include "widgets/text_input.hpp"

#include "common_variable_8x16_sprite_font.h"
#include "bn_sprite_items_silhouette.h"

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
constexpr int ROW_POKERUS  = 7;
constexpr int IDENTITY_ROW_COUNT = 8;
constexpr int VIEWPORT_ROWS      = 5;

// STATS tab row layout: 1 level + 6 EVs + 6 IVs = 13.
constexpr int ROW_LEVEL    = 0;
constexpr int ROW_EV_HP    = 1;
constexpr int ROW_EV_ATK   = 2;
constexpr int ROW_EV_DEF   = 3;
constexpr int ROW_EV_SPA   = 4;
constexpr int ROW_EV_SPD   = 5;
constexpr int ROW_EV_SPE   = 6;
constexpr int ROW_IV_HP    = 7;
constexpr int ROW_IV_ATK   = 8;
constexpr int ROW_IV_DEF   = 9;
constexpr int ROW_IV_SPA   = 10;
constexpr int ROW_IV_SPD   = 11;
constexpr int ROW_IV_SPE   = 12;
constexpr int STATS_ROW_COUNT = 13;

constexpr int LEVEL_MIN = 1;
constexpr int LEVEL_MAX = 100;

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

// MOVES tab: 4 (move, PP) pairs.
constexpr int ROW_MOVE_0 = 0;
constexpr int ROW_PP_0   = 1;
constexpr int ROW_MOVE_1 = 2;
constexpr int ROW_PP_1   = 3;
constexpr int ROW_MOVE_2 = 4;
constexpr int ROW_PP_2   = 5;
constexpr int ROW_MOVE_3 = 6;
constexpr int ROW_PP_3   = 7;
constexpr int MOVES_ROW_COUNT = 8;

// Gen 3 move ID space: 0 (empty slot) .. 354 (Psycho Boost).
constexpr int MOVE_MIN = 0;
constexpr int MOVE_MAX = 354;

constexpr int PP_UP_NUMERATOR   = 8;
constexpr int PP_UP_DENOMINATOR = 5;

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
    , level_(LEVEL_MIN, LEVEL_MIN, LEVEL_MAX)
    // EV/IV ranges are seeded properly in rebuild_for_current_tab() once
    // we know the game variant; the placeholder (0, 0, 252/31) here is
    // valid for Gen 3.
    , ev_hp_(0, 0, 252), ev_atk_(0, 0, 252), ev_def_(0, 0, 252)
    , ev_spa_(0, 0, 252), ev_spd_(0, 0, 252), ev_spe_(0, 0, 252)
    , iv_hp_(0, 0, 31),  iv_atk_(0, 0, 31),  iv_def_(0, 0, 31)
    , iv_spa_(0, 0, 31), iv_spd_(0, 0, 31),  iv_spe_(0, 0, 31)
    , move_0_(MOVE_MIN, MOVE_MIN, MOVE_MAX)
    , move_1_(MOVE_MIN, MOVE_MIN, MOVE_MAX)
    , move_2_(MOVE_MIN, MOVE_MIN, MOVE_MAX)
    , move_3_(MOVE_MIN, MOVE_MIN, MOVE_MAX)
    // PP caps are reseated per slot in load_from_save() based on the
    // current move id; the ctor seeds them to 0/0/0 (empty slot).
    , pp_0_(0, 0, 0), pp_1_(0, 0, 0), pp_2_(0, 0, 0), pp_3_(0, 0, 0)
    , pokemon_{}
    // Source resolution: a non-default pending_box_* pair means we were
    // entered from box_scene; otherwise fall back to party.  Matches the
    // bag pattern (pending_bag_* defaults to NONE/-1 when stale).
    , source_(bridge.pending_box_index() >= 0
              ? edit_source::BOX
              : edit_source::PARTY)
    , slot_(source_ == edit_source::BOX
            ? bridge.pending_box_slot()
            : bridge.pending_party_slot())
    , box_(source_ == edit_source::BOX
           ? bridge.pending_box_index()
           : 0)
    , tab_(edit_tab::IDENTITY)
    , mode_(edit_mode::NAV)
{
    // Defensive: callers always set the right pending-* fields before
    // transitioning here.  Clamp out-of-range so the scene still
    // constructs cleanly.
    if (source_ == edit_source::BOX)
    {
        if (slot_ < 0 || slot_ >= 30) slot_ = 0;
        if (box_  < 0)                box_  = 0;
    }
    else
    {
        if (slot_ < 0 || slot_ >= 6) slot_ = 0;
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
        case edit_tab::STATS:    return STATS_ROW_COUNT;
        case edit_tab::MOVES:    return MOVES_ROW_COUNT;
        default:                 return 0;
    }
}

void pokemon_edit_scene::load_from_save()
{
    if (!bridge_.ok())
    {
        return;
    }
    if (source_ == edit_source::BOX)
    {
        pksave_pokemon_t box[30] = {};
        uint8_t count = 0;
        ::pksave_get_box(bridge_.handle(),
                         static_cast<uint8_t>(box_),
                         box, &count);
        pokemon_ = box[slot_];
    }
    else
    {
        pksave_pokemon_t party[6] = {};
        uint8_t count = 0;
        ::pksave_get_party(bridge_.handle(), party, &count);
        pokemon_ = party[slot_];
    }

    const int ev_cap = ::pksave_ev_max_per_stat(bridge_.game());
    const int iv_cap = ::pksave_iv_max(bridge_.game());

    level_.set_value(pokemon_.level);
    // Re-construct numeric_field caps in-place by writing through
    // set_value clamped to a temporarily-known range — but
    // numeric_field doesn't expose set_range.  Instead we rely on the
    // ctor's range (0..252 / 0..31), which matches Gen 3 caps.
    // If Gen 4/5 ever uses this scene, switch to a setter.
    (void)ev_cap;
    (void)iv_cap;

    // libpksave's evs[]/ivs[]/stat[] arrays are indexed HP, Atk, Def,
    // SpA, SpD, Spe — see libpksave/src/recalc.c's `bases[6]` literal
    // for the canonical order.
    ev_hp_.set_value(pokemon_.evs[0]);
    ev_atk_.set_value(pokemon_.evs[1]);
    ev_def_.set_value(pokemon_.evs[2]);
    ev_spa_.set_value(pokemon_.evs[3]);
    ev_spd_.set_value(pokemon_.evs[4]);
    ev_spe_.set_value(pokemon_.evs[5]);

    iv_hp_.set_value(pokemon_.ivs[0]);
    iv_atk_.set_value(pokemon_.ivs[1]);
    iv_def_.set_value(pokemon_.ivs[2]);
    iv_spa_.set_value(pokemon_.ivs[3]);
    iv_spd_.set_value(pokemon_.ivs[4]);
    iv_spe_.set_value(pokemon_.ivs[5]);

    for (int slot = 0; slot < 4; ++slot)
    {
        const int id = pokemon_.move_ids[slot];
        cycler* mc = move_cycler_for_slot(slot);
        numeric_field* nf = pp_field_for_slot(slot);
        if (mc != nullptr) mc->set_id(id);
        if (nf != nullptr)
        {
            nf->set_max(pp_cap_for_slot(slot));
            nf->set_value(pokemon_.move_pp[slot]);
        }
    }
}

void pokemon_edit_scene::push_to_save()
{
    if (!bridge_.ok())
    {
        return;
    }
    if (source_ == edit_source::BOX)
    {
        int rc = ::pksave_set_box_slot(bridge_.handle(),
                                       static_cast<uint8_t>(box_),
                                       static_cast<uint8_t>(slot_),
                                       &pokemon_);
        BN_LOG("edit: push box=", box_, " slot=", slot_,
               " sp=", pokemon_.species, " rc=", rc);
        bridge_.mark_dirty();

        pksave_pokemon_t verify[30] = {};
        uint8_t cnt = 0;
        ::pksave_get_box(bridge_.handle(),
                         static_cast<uint8_t>(box_),
                         verify, &cnt);
        BN_LOG("edit: verify sp=", verify[slot_].species, " cnt=", cnt);
    }
    else
    {
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
    constexpr int y_top   = -56;
    constexpr int row_h   = 12;

    if (tab_ == edit_tab::STATS)
    {
        // STATS owns the right panel; drop the mon sprite so the stat
        // list has the full vertical space.
        mon_sprite_.reset();
        // Live calculated stats from pksave_recalc_stats.  The caller
        // is responsible for invoking recalc before redraw — every
        // EV/IV/level commit path does so.
        bn::string<24> species_line;
        species_line.append(::pksave_species_name(bridge_.game(),
                                                  pokemon_.species));
        text_gen_.generate(x_right, y_top + 0 * row_h, species_line,
                           right_panel_sprites_);
        text_gen_.generate(x_right, y_top + 1 * row_h,
            bn::format<16>("HP {}",  pokemon_.stat[0]),
            right_panel_sprites_);
        text_gen_.generate(x_right, y_top + 2 * row_h,
            bn::format<16>("At {}",  pokemon_.stat[1]),
            right_panel_sprites_);
        text_gen_.generate(x_right, y_top + 3 * row_h,
            bn::format<16>("Df {}",  pokemon_.stat[2]),
            right_panel_sprites_);
        text_gen_.generate(x_right, y_top + 4 * row_h,
            bn::format<16>("SA {}",  pokemon_.stat[3]),
            right_panel_sprites_);
        text_gen_.generate(x_right, y_top + 5 * row_h,
            bn::format<16>("SD {}",  pokemon_.stat[4]),
            right_panel_sprites_);
        text_gen_.generate(x_right, y_top + 6 * row_h,
            bn::format<16>("Sp {}",  pokemon_.stat[5]),
            right_panel_sprites_);

        // EV total / cap footer.  Per-stat caps are enforced; the
        // total cap is displayed but not enforced — see scene
        // docblock and plan §"Pokemon editor STATS".
        const int total = ev_hp_.value() + ev_atk_.value() + ev_def_.value()
                        + ev_spa_.value() + ev_spd_.value() + ev_spe_.value();
        const int total_cap = ::pksave_ev_total_max(bridge_.game());
        text_gen_.generate(x_right, y_top + 7 * row_h,
            bn::format<24>("EV {}/{}", total, total_cap),
            right_panel_sprites_);
        return;
    }

    // IDENTITY / MOVES anchor: per-species front sprite (when extracted
    // via `make gba SPRITES_ROM=...`), else the generic silhouette.
    // Recreated on every redraw so cycling species swaps the icon in
    // the same frame as the text re-render — see species_sprite_lookup.
    const bn::sprite_item* item =
        sprite_lookup::species_sprite_for(pokemon_.species);
    if (item == nullptr)
    {
        item = &bn::sprite_items::silhouette;
    }
    mon_sprite_ = item->create_sprite(x_right + 32, y_top + 32);
    constexpr int text_y_start = y_top + 64 + 4;
    bn::string<24> species_line;
    species_line.append(::pksave_species_name(bridge_.game(),
                                              pokemon_.species));
    text_gen_.generate(x_right, text_y_start, species_line,
                       right_panel_sprites_);
    text_gen_.generate(x_right, text_y_start + row_h,
                       bn::format<8>("L{}", pokemon_.level),
                       right_panel_sprites_);
}

void pokemon_edit_scene::rebuild_for_current_tab()
{
    list_.set_row_count(row_count_for_tab());

    placeholder_sprites_.clear();

    // Right panel content depends on tab: IDENTITY/MOVES anchor on the
    // species+level summary; STATS shows the calculated stat[6].
    redraw_right_panel();
}

void pokemon_edit_scene::row_provider_thunk(void* ctx, int row,
                                            bn::string<32>& out)
{
    static_cast<pokemon_edit_scene*>(ctx)->format_row(row, out);
}

void pokemon_edit_scene::format_row(int row, bn::string<32>& out)
{
    const bool active_here = (mode_ == edit_mode::ACTIVE) &&
                             (list_.selected() == row);
    const char* lb = active_here ? "<" : " ";
    const char* rb = active_here ? ">" : " ";

    if (tab_ == edit_tab::STATS)
    {
        // STATS labels: per-stat caps are enforced by the underlying
        // numeric_field's range; total-cap is displayed in the right
        // panel (see redraw_right_panel) but not enforced here.
        auto fmt_num = [&](const char* label, int v) {
            out.append(bn::format<28>("{}:{}{}{}", label, lb, v, rb));
        };
        switch (row)
        {
            case ROW_LEVEL:  fmt_num("Lv  ", level_.value()); break;
            case ROW_EV_HP:  fmt_num("EVHp", ev_hp_.value());  break;
            case ROW_EV_ATK: fmt_num("EVAt", ev_atk_.value()); break;
            case ROW_EV_DEF: fmt_num("EVDf", ev_def_.value()); break;
            case ROW_EV_SPA: fmt_num("EVSA", ev_spa_.value()); break;
            case ROW_EV_SPD: fmt_num("EVSD", ev_spd_.value()); break;
            case ROW_EV_SPE: fmt_num("EVSp", ev_spe_.value()); break;
            case ROW_IV_HP:  fmt_num("IVHp", iv_hp_.value());  break;
            case ROW_IV_ATK: fmt_num("IVAt", iv_atk_.value()); break;
            case ROW_IV_DEF: fmt_num("IVDf", iv_def_.value()); break;
            case ROW_IV_SPA: fmt_num("IVSA", iv_spa_.value()); break;
            case ROW_IV_SPD: fmt_num("IVSD", iv_spd_.value()); break;
            case ROW_IV_SPE: fmt_num("IVSp", iv_spe_.value()); break;
            default: break;
        }
        return;
    }

    if (tab_ == edit_tab::MOVES)
    {
        const int move_slot = move_slot_for_row(row);
        if (move_slot >= 0)
        {
            const int id = pokemon_.move_ids[move_slot];
            const char* name = (id == 0)
                ? "(empty)"
                : ::pksave_move_name(bridge_.game(),
                                     static_cast<uint16_t>(id));
            out.append(bn::format<28>("M{}  :{}{}{}",
                                      move_slot + 1, lb, name, rb));
            return;
        }
        const int pp_slot = pp_slot_for_row(row);
        if (pp_slot >= 0)
        {
            const int id = pokemon_.move_ids[pp_slot];
            if (id == 0)
            {
                out.append(bn::format<24>("PP{} : -/-", pp_slot + 1));
            }
            else
            {
                const int cap = pp_cap_for_slot(pp_slot);
                out.append(bn::format<28>("PP{} :{}{}{}/{}",
                                          pp_slot + 1, lb,
                                          static_cast<int>(pokemon_.move_pp[pp_slot]),
                                          rb, cap));
            }
        }
        return;
    }

    if (tab_ != edit_tab::IDENTITY) return;

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
        case ROW_POKERUS:
        {
            pksave_pokerus_t pkrs = {};
            ::pksave_get_party_pokerus(bridge_.handle(),
                                       static_cast<uint8_t>(slot_),
                                       &pkrs);
            switch (pkrs.status)
            {
                case PKSAVE_PKRS_NEVER:
                    out.append("Pkrs : Never");
                    break;
                case PKSAVE_PKRS_ACTIVE:
                    out.append(bn::format<28>("Pkrs : Active s={} d={}",
                                              static_cast<int>(pkrs.strain),
                                              static_cast<int>(pkrs.days)));
                    break;
                case PKSAVE_PKRS_CURED:
                    out.append(bn::format<28>("Pkrs : Cured s={}",
                                              static_cast<int>(pkrs.strain)));
                    break;
                default: break;
            }
            break;
        }
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
    if (tab_ == edit_tab::IDENTITY)
    {
        switch (sel)
        {
            case ROW_SPECIES: species_.on_focus_acquired(); break;
            case ROW_ITEM:    item_.on_focus_acquired();    break;
            case ROW_ABILITY: ability_.on_focus_acquired(); break;
            case ROW_NATURE:  nature_.on_focus_acquired();  break;
            default: break;
        }
    }
    else if (tab_ == edit_tab::STATS)
    {
        // Mirror trainer_scene's disarm pattern: the A-press driving
        // the mode transition is already debounced (a_pressed is an
        // edge), but the d-pad axis that may have brought focus here
        // could still be held.  The numeric_field's on_focus_acquired
        // disarms any held axis until released.
        if (numeric_field* nf = stats_field_for_row(sel))
        {
            nf->on_focus_acquired();
        }
    }
    else if (tab_ == edit_tab::MOVES)
    {
        const int move_slot = move_slot_for_row(sel);
        if (move_slot >= 0)
        {
            if (cycler* mc = move_cycler_for_slot(move_slot))
            {
                mc->on_focus_acquired();
            }
        }
        else
        {
            const int pp_slot = pp_slot_for_row(sel);
            if (pp_slot >= 0)
            {
                if (numeric_field* nf = pp_field_for_slot(pp_slot))
                {
                    nf->on_focus_acquired();
                }
            }
        }
    }
    BN_LOG("edit: mode=ACTIVE row=", sel);
}

numeric_field* pokemon_edit_scene::stats_field_for_row(int row)
{
    switch (row)
    {
        case ROW_LEVEL:  return &level_;
        case ROW_EV_HP:  return &ev_hp_;
        case ROW_EV_ATK: return &ev_atk_;
        case ROW_EV_DEF: return &ev_def_;
        case ROW_EV_SPA: return &ev_spa_;
        case ROW_EV_SPD: return &ev_spd_;
        case ROW_EV_SPE: return &ev_spe_;
        case ROW_IV_HP:  return &iv_hp_;
        case ROW_IV_ATK: return &iv_atk_;
        case ROW_IV_DEF: return &iv_def_;
        case ROW_IV_SPA: return &iv_spa_;
        case ROW_IV_SPD: return &iv_spd_;
        case ROW_IV_SPE: return &iv_spe_;
        default:         return nullptr;
    }
}

int pokemon_edit_scene::move_slot_for_row(int row) const
{
    switch (row)
    {
        case ROW_MOVE_0: return 0;
        case ROW_MOVE_1: return 1;
        case ROW_MOVE_2: return 2;
        case ROW_MOVE_3: return 3;
        default:         return -1;
    }
}

int pokemon_edit_scene::pp_slot_for_row(int row) const
{
    switch (row)
    {
        case ROW_PP_0: return 0;
        case ROW_PP_1: return 1;
        case ROW_PP_2: return 2;
        case ROW_PP_3: return 3;
        default:       return -1;
    }
}

int pokemon_edit_scene::pp_cap_for_slot(int slot) const
{
    const int id = pokemon_.move_ids[slot];
    if (id == 0) return 0;
    const int base = ::pksave_move_base_pp(bridge_.game(),
                                           static_cast<uint16_t>(id));
    return (base * PP_UP_NUMERATOR) / PP_UP_DENOMINATOR;
}

cycler* pokemon_edit_scene::move_cycler_for_slot(int slot)
{
    switch (slot)
    {
        case 0: return &move_0_;
        case 1: return &move_1_;
        case 2: return &move_2_;
        case 3: return &move_3_;
        default: return nullptr;
    }
}

numeric_field* pokemon_edit_scene::pp_field_for_slot(int slot)
{
    switch (slot)
    {
        case 0: return &pp_0_;
        case 1: return &pp_1_;
        case 2: return &pp_2_;
        case 3: return &pp_3_;
        default: return nullptr;
    }
}

void pokemon_edit_scene::commit_move_slot(int slot)
{
    cycler* mc = move_cycler_for_slot(slot);
    if (mc == nullptr) return;
    const int old_id = pokemon_.move_ids[slot];
    const int new_id = mc->id();
    pokemon_.move_ids[slot] = static_cast<uint8_t>(new_id);

    numeric_field* nf = pp_field_for_slot(slot);
    if (nf != nullptr)
    {
        const int new_cap = pp_cap_for_slot(slot);
        nf->set_max(new_cap);
        if (new_id == 0)
        {
            nf->set_value(0);
        }
        else if (old_id == 0)
        {
            nf->set_value(new_cap);
        }
        else if (nf->value() > new_cap)
        {
            nf->set_value(new_cap);
        }
        pokemon_.move_pp[slot] = static_cast<uint8_t>(nf->value());
    }
    push_to_save();
}

void pokemon_edit_scene::commit_pp_slot(int slot)
{
    numeric_field* nf = pp_field_for_slot(slot);
    if (nf == nullptr) return;
    pokemon_.move_pp[slot] = static_cast<uint8_t>(nf->value());
    push_to_save();
}

void pokemon_edit_scene::commit_stats_row(int row)
{
    switch (row)
    {
        case ROW_LEVEL:
            pokemon_.level = static_cast<uint8_t>(level_.value());
            break;
        case ROW_EV_HP:  pokemon_.evs[0] = static_cast<uint16_t>(ev_hp_.value());  break;
        case ROW_EV_ATK: pokemon_.evs[1] = static_cast<uint16_t>(ev_atk_.value()); break;
        case ROW_EV_DEF: pokemon_.evs[2] = static_cast<uint16_t>(ev_def_.value()); break;
        case ROW_EV_SPA: pokemon_.evs[3] = static_cast<uint16_t>(ev_spa_.value()); break;
        case ROW_EV_SPD: pokemon_.evs[4] = static_cast<uint16_t>(ev_spd_.value()); break;
        case ROW_EV_SPE: pokemon_.evs[5] = static_cast<uint16_t>(ev_spe_.value()); break;
        case ROW_IV_HP:  pokemon_.ivs[0] = static_cast<uint8_t>(iv_hp_.value());   break;
        case ROW_IV_ATK: pokemon_.ivs[1] = static_cast<uint8_t>(iv_atk_.value());  break;
        case ROW_IV_DEF: pokemon_.ivs[2] = static_cast<uint8_t>(iv_def_.value());  break;
        case ROW_IV_SPA: pokemon_.ivs[3] = static_cast<uint8_t>(iv_spa_.value());  break;
        case ROW_IV_SPD: pokemon_.ivs[4] = static_cast<uint8_t>(iv_spd_.value());  break;
        case ROW_IV_SPE: pokemon_.ivs[5] = static_cast<uint8_t>(iv_spe_.value());  break;
        default: return;
    }
    // Run recalc BEFORE the next render so the right-panel calculated
    // stats reflect the new EV/IV/level in the same frame.
    ::pksave_recalc_stats(&pokemon_, bridge_.game());
    push_to_save();
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

    // B handling: in ACTIVE, exit ACTIVE (no revert); in NAV, return to
    // the entry-source scene (PARTY or BOXES).
    if (bn::keypad::b_pressed())
    {
        if (mode_ == edit_mode::ACTIVE)
        {
            exit_active();
            list_.redraw();
            return bn::nullopt;
        }
        return source_ == edit_source::BOX
               ? scene_kind::BOXES
               : scene_kind::PARTY;
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
    if (mode_ == edit_mode::NAV)
    {
        const bool selection_changed = list_.tick();
        const int sel = list_.selected();

        if (selection_changed)
        {
            if (tab_ == edit_tab::IDENTITY)
            {
                switch (sel)
                {
                    case ROW_SPECIES: species_.on_focus_acquired(); break;
                    case ROW_ITEM:    item_.on_focus_acquired();    break;
                    case ROW_ABILITY: ability_.on_focus_acquired(); break;
                    case ROW_NATURE:  nature_.on_focus_acquired();  break;
                    default: break;
                }
            }
            else if (tab_ == edit_tab::STATS)
            {
                if (numeric_field* nf = stats_field_for_row(sel))
                {
                    nf->on_focus_acquired();
                }
            }
            else  // MOVES
            {
                const int move_slot = move_slot_for_row(sel);
                if (move_slot >= 0)
                {
                    if (cycler* mc = move_cycler_for_slot(move_slot))
                        mc->on_focus_acquired();
                }
                else
                {
                    const int pp_slot = pp_slot_for_row(sel);
                    if (pp_slot >= 0)
                    {
                        if (numeric_field* nf = pp_field_for_slot(pp_slot))
                            nf->on_focus_acquired();
                    }
                }
            }
            need_list_redraw = true;
        }
    }

    const int sel = list_.selected();

    // A handling depends on mode, tab, and row.
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
                case ROW_POKERUS:
                    // Pokerus from a box-resident Pokemon is a CP#5
                    // deferred follow-up (pokerus_modal_scene currently
                    // keys on pending_party_slot only; the
                    // pksave_{get,set}_box_pokerus APIs exist, just need
                    // wiring + 2 tests).  Treat as no-op here so the box
                    // entry path doesn't crash or misdirect into a party
                    // slot.
                    if (source_ == edit_source::BOX)
                    {
                        break;
                    }
                    // Re-set pending_party_slot so the modal can read
                    // the same slot.  party_scene set it on entry; the
                    // bridge keeps it across scene transitions, but
                    // re-stamping here makes the dependency explicit
                    // and survives someone refactoring party_scene.
                    bridge_.set_pending_party_slot(slot_);
                    return scene_kind::POKERUS_MODAL;
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
    else if (bn::keypad::a_pressed() && tab_ == edit_tab::STATS)
    {
        if (mode_ == edit_mode::NAV)
        {
            if (stats_field_for_row(sel) != nullptr)
            {
                enter_active();
                need_list_redraw = true;
            }
        }
        else // ACTIVE: A commits — push, recalc, redraw.
        {
            commit_stats_row(sel);
            exit_active();
            need_list_redraw  = true;
            need_right_redraw = true;
        }
    }
    else if (bn::keypad::a_pressed() && tab_ == edit_tab::MOVES)
    {
        if (mode_ == edit_mode::NAV)
        {
            const int move_slot = move_slot_for_row(sel);
            const int pp_slot   = pp_slot_for_row(sel);
            if (move_slot >= 0)
            {
                enter_active();
                need_list_redraw = true;
            }
            else if (pp_slot >= 0 && pokemon_.move_ids[pp_slot] != 0)
            {
                // Empty-move PP is non-editable; A on an empty PP row
                // is a no-op (visually rendered as -/-).
                enter_active();
                need_list_redraw = true;
            }
        }
        else // ACTIVE: A commits.
        {
            const int move_slot = move_slot_for_row(sel);
            const int pp_slot   = pp_slot_for_row(sel);
            if (move_slot >= 0)
            {
                commit_move_slot(move_slot);
            }
            else if (pp_slot >= 0)
            {
                commit_pp_slot(pp_slot);
            }
            exit_active();
            need_list_redraw  = true;
            need_right_redraw = true;
        }
    }

    // Per-row STATS numeric tick (ACTIVE-only).  Every tick that
    // changes a value pushes pokemon_, recalcs stats, and redraws.
    if (mode_ == edit_mode::ACTIVE && tab_ == edit_tab::STATS)
    {
        if (numeric_field* nf = stats_field_for_row(sel))
        {
            if (nf->tick())
            {
                commit_stats_row(sel);
                need_list_redraw   = true;
                need_right_redraw  = true;
                need_header_redraw = true;
            }
        }
    }

    // Per-row MOVES tick (ACTIVE-only).  Move cyclers update id only;
    // the move row commits on A-press (handled above) — this keeps the
    // cycle preview live but doesn't flash-write per tick.  The PP
    // numeric tick updates value live; commit on A.
    if (mode_ == edit_mode::ACTIVE && tab_ == edit_tab::MOVES)
    {
        const int move_slot = move_slot_for_row(sel);
        const int pp_slot   = pp_slot_for_row(sel);
        if (move_slot >= 0)
        {
            if (cycler* mc = move_cycler_for_slot(move_slot))
            {
                if (mc->tick())
                {
                    need_list_redraw = true;
                }
            }
        }
        else if (pp_slot >= 0 && pokemon_.move_ids[pp_slot] != 0)
        {
            if (numeric_field* nf = pp_field_for_slot(pp_slot))
            {
                if (nf->tick())
                {
                    need_list_redraw = true;
                }
            }
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
