// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "trainer_scene.hpp"
#include "widgets/text_input.hpp"

#include "common_variable_8x16_sprite_font.h"

#include <bn_keypad.h>
#include <bn_format.h>

#include <cstring>

namespace pksave_gba {
namespace {

// Row indices, derived once for readability and to keep the order
// changeable in one place.
constexpr int ROW_NAME    = 0;
constexpr int ROW_TID     = 1;
constexpr int ROW_SID     = 2;
constexpr int ROW_MONEY   = 3;
constexpr int ROW_BADGE_0 = 4;
constexpr int ROW_COUNT   = ROW_BADGE_0 + 8;   // 12 total

constexpr int VIEWPORT_ROWS = 7;

// Gen 3 money cap — Ruby/Sapphire/Emerald/FRLG all share 999,999.
constexpr int MONEY_MAX = 999'999;

}  // namespace

trainer_scene::trainer_scene(pksave_bridge& bridge)
    : bridge_(bridge)
    , text_gen_(common::variable_8x16_sprite_font)
    , list_(text_gen_, ROW_COUNT, VIEWPORT_ROWS,
            /*x=*/-104, /*y_top=*/-40, /*row_h=*/16)
    , trainer_{}
    , tid_(0, 0, 65535)
    , sid_(0, 0, 65535)
    , money_(0, 0, MONEY_MAX)
{
    load_from_save();
    // If we got here from a text_input return, apply the result before
    // drawing.  consume_text_input_result() also marks the save dirty
    // when appropriate.
    consume_text_input_result();

    list_.set_row_provider(&trainer_scene::row_provider_thunk, this);
    redraw_header();
}

void trainer_scene::load_from_save()
{
    if (!bridge_.ok())
    {
        return;
    }
    ::pksave_get_trainer(bridge_.handle(), &trainer_);
    tid_.set_value(trainer_.tid);
    sid_.set_value(trainer_.sid);
    money_.set_value(static_cast<int>(trainer_.money));
}

void trainer_scene::push_to_save()
{
    if (!bridge_.ok())
    {
        return;
    }
    trainer_.tid   = static_cast<uint16_t>(tid_.value());
    trainer_.sid   = static_cast<uint16_t>(sid_.value());
    trainer_.money = static_cast<uint32_t>(money_.value());
    ::pksave_set_trainer(bridge_.handle(), &trainer_);
    bridge_.mark_dirty();
}

void trainer_scene::consume_text_input_result()
{
    auto& mbx = text_input_state();
    if (!mbx.result.valid)
    {
        return;
    }
    mbx.result.valid = false;   // single-shot consumption

    if (mbx.result.cancelled)
    {
        return;
    }

    // Copy committed name into the trainer struct.  `trainer_.name`
    // is 13 bytes; libpksave's encoder reads up to max-len from a
    // null-terminated buffer, so we zero-fill first.
    std::memset(trainer_.name, 0, sizeof(trainer_.name));
    const int len = bn::min<int>(mbx.result.value.size(),
                                  PKSAVE_GEN3_TRAINER_NAME_MAX);
    for (int i = 0; i < len; ++i)
    {
        trainer_.name[i] = mbx.result.value[i];
    }
    // push: writes name + everything else (idempotent on unchanged fields).
    push_to_save();
}

void trainer_scene::redraw_header()
{
    header_sprites_.clear();
    text_gen_.set_center_alignment();

    bn::string<32> header;
    header.append("TRAINER");
    if (bridge_.is_dirty())
    {
        header.append(" *");
    }
    text_gen_.generate(0, -72, header, header_sprites_);
}

void trainer_scene::row_provider_thunk(void* ctx, int row, bn::string<32>& out)
{
    static_cast<trainer_scene*>(ctx)->format_row(row, out);
}

void trainer_scene::format_row(int row, bn::string<32>& out)
{
    switch (row)
    {
        case ROW_NAME:
        {
            out.append("Name : ");
            // Copy up to MAX trainer-name chars.
            for (int i = 0;
                 i < PKSAVE_GEN3_TRAINER_NAME_MAX && trainer_.name[i] != '\0';
                 ++i)
            {
                out.append(trainer_.name[i]);
            }
            break;
        }
        case ROW_TID:
            out.append(bn::format<24>("TID  : {}", tid_.value()));
            break;
        case ROW_SID:
            out.append(bn::format<24>("SID  : {}", sid_.value()));
            break;
        case ROW_MONEY:
            out.append(bn::format<24>("Money: {}", money_.value()));
            break;
        default:
        {
            // Badge rows.
            const int b = row - ROW_BADGE_0;
            const bool on = (trainer_.badges >> b) & 1;
            out.append(bn::format<24>("Badge {}: {}",
                                      b + 1, on ? "ON" : "off"));
            break;
        }
    }
}

bn::optional<scene_kind> trainer_scene::update()
{
    bool need_header_redraw = false;
    bool need_list_redraw = false;

    // B exits to main menu.
    if (bn::keypad::b_pressed())
    {
        return scene_kind::MAIN_MENU;
    }

    // List navigation (up/down).
    const bool selection_changed = list_.tick();
    const int sel = list_.selected();

    if (selection_changed)
    {
        // If we just landed on a numeric_field row, the d-pad edge that
        // drove the navigation is still being held. Disarm the field so
        // it doesn't reinterpret that hold as a fresh "frame 1" step.
        // See numeric_field::on_focus_acquired() + test_tid_roundtrip.py.
        switch (sel)
        {
            case ROW_TID:   tid_.on_focus_acquired();   break;
            case ROW_SID:   sid_.on_focus_acquired();   break;
            case ROW_MONEY: money_.on_focus_acquired(); break;
            default: break;
        }
        need_list_redraw = true;
    }

    // Per-field input handling.
    if (sel == ROW_NAME)
    {
        if (bn::keypad::a_pressed())
        {
            // Open the text_input keyboard.  Populate the mailbox
            // with the current name and the return scene.
            auto& mbx = text_input_state();
            mbx.request.initial.clear();
            for (int i = 0;
                 i < PKSAVE_GEN3_TRAINER_NAME_MAX && trainer_.name[i] != '\0';
                 ++i)
            {
                mbx.request.initial.append(trainer_.name[i]);
            }
            mbx.request.max_len   = PKSAVE_GEN3_TRAINER_NAME_MAX;
            mbx.request.return_to = scene_kind::TRAINER;
            mbx.result.valid      = false;
            return scene_kind::TEXT_INPUT;
        }
    }
    else if (sel == ROW_TID)
    {
        if (tid_.tick())
        {
            push_to_save();
            need_list_redraw = true;
            need_header_redraw = true;  // dirty flag may have flipped
        }
    }
    else if (sel == ROW_SID)
    {
        if (sid_.tick())
        {
            push_to_save();
            need_list_redraw = true;
            need_header_redraw = true;
        }
    }
    else if (sel == ROW_MONEY)
    {
        if (money_.tick())
        {
            push_to_save();
            need_list_redraw = true;
            need_header_redraw = true;
        }
    }
    else
    {
        // Badge row — A toggles.
        if (bn::keypad::a_pressed())
        {
            const int b = sel - ROW_BADGE_0;
            trainer_.badges ^= static_cast<uint8_t>(1u << b);
            push_to_save();
            need_list_redraw = true;
            need_header_redraw = true;
        }
    }

    if (need_list_redraw)
    {
        list_.redraw();
    }
    if (need_header_redraw)
    {
        redraw_header();
    }

    return bn::nullopt;
}

}  // namespace pksave_gba
