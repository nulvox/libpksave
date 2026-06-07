// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "pokerus_modal_scene.hpp"

#include "common_variable_8x16_sprite_font.h"

#include <bn_keypad.h>
#include <bn_log.h>
#include <bn_format.h>

namespace pksave_gba {
namespace {

constexpr int ROW_STATUS = 0;
constexpr int ROW_STRAIN = 1;
constexpr int ROW_DAYS   = 2;

constexpr int STRAIN_MIN = 1;
constexpr int STRAIN_MAX = 15;
constexpr int DAYS_MIN   = 0;
constexpr int DAYS_MAX   = 15;

}  // namespace

pokerus_modal_scene::pokerus_modal_scene(pksave_bridge& bridge)
    : bridge_(bridge)
    , text_gen_(common::variable_8x16_sprite_font)
    , slot_(bridge.pending_party_slot())
    , status_(PKSAVE_PKRS_NEVER)
    , strain_(0)
    , days_(0)
    , cursor_(ROW_STATUS)
{
    if (slot_ < 0 || slot_ >= 6)
    {
        slot_ = 0;
    }
    load_from_save();
    redraw();

    // Scene-entry breadcrumb for the mGBA-Lua harness.
    BN_LOG("pokerus: OK");
    BN_LOG("pokerus: slot=", slot_,
           " status=", static_cast<int>(status_),
           " strain=", static_cast<int>(strain_),
           " days=", static_cast<int>(days_));
}

void pokerus_modal_scene::load_from_save()
{
    if (!bridge_.ok())
    {
        return;
    }
    pksave_pokerus_t pkrs = {};
    int rc = ::pksave_get_party_pokerus(bridge_.handle(),
                                        static_cast<uint8_t>(slot_),
                                        &pkrs);
    if (rc != PKSAVE_OK)
    {
        return;
    }
    status_ = pkrs.status;
    strain_ = pkrs.strain;
    days_   = pkrs.days;
}

void pokerus_modal_scene::commit_to_save()
{
    if (!bridge_.ok())
    {
        return;
    }
    // Normalise (strain, days) to the canonical encoding of the
    // current status before writing.  pksave_set_party_pokerus
    // accepts (strain, days) as raw nibbles; the status_ machine is
    // a frontend convenience.
    uint8_t strain = strain_;
    uint8_t days   = days_;
    switch (status_)
    {
        case PKSAVE_PKRS_NEVER:
            strain = 0;
            days   = 0;
            break;
        case PKSAVE_PKRS_ACTIVE:
            // strain in [1,15], days in [1,15] (0 days while strain>0
            // means CURED — disambiguate by clamping days up).
            if (strain == 0) strain = 1;
            if (days   == 0) days   = 1;
            break;
        case PKSAVE_PKRS_CURED:
            if (strain == 0) strain = 1;
            days = 0;
            break;
        default: break;
    }
    int rc = ::pksave_set_party_pokerus(bridge_.handle(),
                                        static_cast<uint8_t>(slot_),
                                        strain, days);
    BN_LOG("pokerus: commit slot=", slot_,
           " strain=", static_cast<int>(strain),
           " days=",   static_cast<int>(days),
           " rc=",     rc);
    bridge_.mark_dirty();
}

int pokerus_modal_scene::row_count_visible() const
{
    switch (status_)
    {
        case PKSAVE_PKRS_NEVER:  return 1;            // STATUS only
        case PKSAVE_PKRS_ACTIVE: return 3;            // STATUS+STRAIN+DAYS
        case PKSAVE_PKRS_CURED:  return 2;            // STATUS+STRAIN
        default:                 return 1;
    }
}

void pokerus_modal_scene::cycle_status(int direction)
{
    int next = static_cast<int>(status_) + direction;
    // Cycle through NEVER(0) / ACTIVE(1) / CURED(2) with wrap.
    next = ((next % 3) + 3) % 3;
    pksave_pokerus_status_t new_status = static_cast<pksave_pokerus_status_t>(next);
    if (new_status == status_) return;

    // Side effects per docblock state transitions.
    switch (new_status)
    {
        case PKSAVE_PKRS_NEVER:
            // Keep strain in struct, render dashes via status filter
            // in redraw().  Days is always 0 for NEVER.
            days_ = 0;
            break;
        case PKSAVE_PKRS_ACTIVE:
            if (strain_ == 0) strain_ = 1;
            if (days_   == 0) days_   = 1;
            break;
        case PKSAVE_PKRS_CURED:
            // CURED means days has counted down — clamp to 0; keep
            // strain so the user still sees their pick.
            if (strain_ == 0) strain_ = 1;
            days_ = 0;
            break;
        default: break;
    }
    status_ = new_status;

    // If cursor was on a row that just disappeared, retreat.
    if (cursor_ >= row_count_visible())
    {
        cursor_ = row_count_visible() - 1;
    }
}

void pokerus_modal_scene::step_strain(int delta)
{
    int v = static_cast<int>(strain_) + delta;
    if (v < STRAIN_MIN) v = STRAIN_MIN;
    if (v > STRAIN_MAX) v = STRAIN_MAX;
    strain_ = static_cast<uint8_t>(v);
}

void pokerus_modal_scene::step_days(int delta)
{
    int v = static_cast<int>(days_) + delta;
    if (v < DAYS_MIN) v = DAYS_MIN;
    if (v > DAYS_MAX) v = DAYS_MAX;
    days_ = static_cast<uint8_t>(v);
}

void pokerus_modal_scene::redraw()
{
    sprites_.clear();
    text_gen_.set_left_alignment();

    const int x = -80;
    const int y_top = -48;
    const int row_h = 16;

    // Header
    text_gen_.set_center_alignment();
    text_gen_.generate(0, y_top - 16, "POKERUS", sprites_);
    text_gen_.set_left_alignment();

    // STATUS row
    {
        const char* lb = (cursor_ == ROW_STATUS) ? "<" : " ";
        const char* rb = (cursor_ == ROW_STATUS) ? ">" : " ";
        const char* name =
            (status_ == PKSAVE_PKRS_NEVER)  ? "NEVER" :
            (status_ == PKSAVE_PKRS_ACTIVE) ? "ACTIVE" : "CURED";
        bn::string<28> line;
        line.append(bn::format<28>("Status:{}{}{}", lb, name, rb));
        text_gen_.generate(x, y_top + 0 * row_h, line, sprites_);
    }

    // STRAIN row (visible unless status == NEVER)
    if (status_ != PKSAVE_PKRS_NEVER)
    {
        const char* lb = (cursor_ == ROW_STRAIN) ? "<" : " ";
        const char* rb = (cursor_ == ROW_STRAIN) ? ">" : " ";
        bn::string<28> line;
        line.append(bn::format<28>("Strain:{}{}{}",
                                   lb, static_cast<int>(strain_), rb));
        text_gen_.generate(x, y_top + 1 * row_h, line, sprites_);
    }

    // DAYS row (visible only for ACTIVE)
    if (status_ == PKSAVE_PKRS_ACTIVE)
    {
        const char* lb = (cursor_ == ROW_DAYS) ? "<" : " ";
        const char* rb = (cursor_ == ROW_DAYS) ? ">" : " ";
        bn::string<28> line;
        line.append(bn::format<28>("Days:  {}{}{}",
                                   lb, static_cast<int>(days_), rb));
        text_gen_.generate(x, y_top + 2 * row_h, line, sprites_);
    }

    // Footer hint
    text_gen_.generate(x, y_top + 4 * row_h,
                       "A:save  B:cancel", sprites_);
}

bn::optional<scene_kind> pokerus_modal_scene::update()
{
    if (bn::keypad::b_pressed())
    {
        BN_LOG("pokerus: cancel");
        return scene_kind::POKEMON_EDIT;
    }
    if (bn::keypad::a_pressed())
    {
        commit_to_save();
        return scene_kind::POKEMON_EDIT;
    }

    bool need_redraw = false;

    // UP/DOWN: cursor over visible rows.
    if (bn::keypad::up_pressed())
    {
        const int n = row_count_visible();
        if (n > 0)
        {
            cursor_ = (cursor_ + n - 1) % n;
            need_redraw = true;
        }
    }
    if (bn::keypad::down_pressed())
    {
        const int n = row_count_visible();
        if (n > 0)
        {
            cursor_ = (cursor_ + 1) % n;
            need_redraw = true;
        }
    }

    // L/R: cycle focused field.  Edge-triggered to keep modal feel
    // predictable — the small value ranges don't justify hold-accel.
    if (bn::keypad::right_pressed())
    {
        switch (cursor_)
        {
            case ROW_STATUS: cycle_status(+1); break;
            case ROW_STRAIN: step_strain(+1); break;
            case ROW_DAYS:   step_days(+1);   break;
            default: break;
        }
        need_redraw = true;
    }
    else if (bn::keypad::left_pressed())
    {
        switch (cursor_)
        {
            case ROW_STATUS: cycle_status(-1); break;
            case ROW_STRAIN: step_strain(-1); break;
            case ROW_DAYS:   step_days(-1);   break;
            default: break;
        }
        need_redraw = true;
    }

    if (need_redraw)
    {
        redraw();
    }
    return bn::nullopt;
}

}  // namespace pksave_gba
