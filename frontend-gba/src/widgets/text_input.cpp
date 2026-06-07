// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "text_input.hpp"
#include "text_input_charset.h"

#include "common_variable_8x16_sprite_font.h"

#include <bn_keypad.h>

namespace pksave_gba {
namespace {

constexpr const char* TAB_NAMES[3] = { "UPPER", "lower", "sym" };

// 5x6 grid layout — 30 cells per tab.  Each tab's character list is at
// most 30 chars; shorter lists leave trailing cells blank (rendered as
// space, not selectable).
constexpr int GRID_COLS = 5;
constexpr int GRID_ROWS = 6;
constexpr int GRID_CELLS = GRID_COLS * GRID_ROWS;

// Pixel layout: top-left of grid, cell pitch.
constexpr int GRID_X0   = -36;
constexpr int GRID_Y0   = -8;
constexpr int GRID_DX   = 16;
constexpr int GRID_DY   = 14;

// Header / buffer / hints positions.
constexpr int Y_TITLE   = -64;
constexpr int Y_BUFFER  = -40;
constexpr int Y_HINTS   = 76;

const char* tab_chars(int t)
{
    switch (t)
    {
        case 0: return PKSAVE_GEN3_CHARSET_UPPER;
        case 1: return PKSAVE_GEN3_CHARSET_LOWER;
        case 2: return PKSAVE_GEN3_CHARSET_SYMBOL;
        default: return PKSAVE_GEN3_CHARSET_UPPER;
    }
}

int tab_len(int t)
{
    // Hard-coded to avoid pulling in the C runtime strlen — the
    // butano link has no libc.  The keyboard charsets are compile-time
    // constants so the lengths are too; update in lockstep with
    // text_input_charset.h.
    switch (t)
    {
        case 0: return sizeof(PKSAVE_GEN3_CHARSET_UPPER)  - 1;
        case 1: return sizeof(PKSAVE_GEN3_CHARSET_LOWER)  - 1;
        case 2: return sizeof(PKSAVE_GEN3_CHARSET_SYMBOL) - 1;
        default: return 0;
    }
}

}  // namespace

text_input_mailbox& text_input_state()
{
    static text_input_mailbox mbx;
    return mbx;
}

text_input_scene::text_input_scene()
    : text_gen_(common::variable_8x16_sprite_font)
    , buffer_()
    , max_len_(0)
    , return_to_(scene_kind::MAIN_MENU)
    , tab_(0)
    , grid_cursor_(0)
{
    auto& mbx = text_input_state();
    buffer_     = mbx.request.initial;
    max_len_    = mbx.request.max_len;
    return_to_  = mbx.request.return_to;
    // Pre-emptively mark the result invalid; populated on commit/cancel.
    mbx.result.valid = false;

    redraw();
}

void text_input_scene::redraw()
{
    sprites_.clear();

    // Title row: keyboard mode + active tab indicator.
    text_gen_.set_center_alignment();
    bn::string<32> title;
    title.append("NAME  (");
    title.append(TAB_NAMES[tab_]);
    title.append(")");
    text_gen_.generate(0, Y_TITLE, title, sprites_);

    // Buffer display with a trailing "_" cursor (visual only; not part
    // of buffer_).  Padding to max_len with dots so the user can see
    // remaining room at a glance.
    bn::string<32> row;
    row.append("[");
    for (int i = 0; i < buffer_.size(); ++i) row.append(buffer_[i]);
    if (buffer_.size() < max_len_) row.append("_");
    int pad_count = max_len_ - buffer_.size() - 1;
    for (int i = 0; i < pad_count; ++i) row.append(".");
    row.append("]");
    text_gen_.generate(0, Y_BUFFER, row, sprites_);

    // Grid rows: 5 columns, up to 6 rows.  Each cell is one character;
    // the focused cell gets brackets around it.
    text_gen_.set_left_alignment();
    const char* chars = tab_chars(tab_);
    const int len = tab_len(tab_);

    for (int idx = 0; idx < GRID_CELLS; ++idx)
    {
        const int col = idx % GRID_COLS;
        const int row_i = idx / GRID_COLS;
        const int x = GRID_X0 + col * GRID_DX;
        const int y = GRID_Y0 + row_i * GRID_DY;

        char cell = (idx < len) ? chars[idx] : ' ';
        bn::string<8> piece;
        if (idx == grid_cursor_)
        {
            piece.append("[");
            piece.append(cell);
            piece.append("]");
        }
        else
        {
            piece.append(" ");
            piece.append(cell);
            piece.append(" ");
        }
        text_gen_.generate(x, y, piece, sprites_);
    }

    // Footer hint row.
    text_gen_.set_center_alignment();
    text_gen_.generate(0, Y_HINTS,
        "A:place  B:del  SEL:tab  ST:OK",
        sprites_);
}

void text_input_scene::place_focused_char()
{
    if (buffer_.size() >= max_len_) return;
    const char* chars = tab_chars(tab_);
    const int len = tab_len(tab_);
    if (grid_cursor_ >= len) return;  // blank cell, ignore
    char c = chars[grid_cursor_];
    buffer_.append(c);
    redraw();
}

void text_input_scene::cycle_tab()
{
    tab_ = (tab_ + 1) % 3;
    // Keep cursor in range for the new tab; clamp into the valid prefix.
    const int len = tab_len(tab_);
    if (grid_cursor_ >= len)
    {
        grid_cursor_ = len - 1;
        if (grid_cursor_ < 0) grid_cursor_ = 0;
    }
    redraw();
}

void text_input_scene::backspace()
{
    if (buffer_.empty())
    {
        // Per the plan: B with empty buffer backs out (cancel).
        cancel();
        return;
    }
    buffer_.pop_back();
    redraw();
}

void text_input_scene::commit()
{
    auto& mbx = text_input_state();
    mbx.result.value      = buffer_;
    mbx.result.cancelled  = false;
    mbx.result.valid      = true;
    mbx.result.caller_tag = mbx.request.caller_tag;
}

void text_input_scene::cancel()
{
    auto& mbx = text_input_state();
    mbx.result.value      = buffer_;
    mbx.result.cancelled  = true;
    mbx.result.valid      = true;
    mbx.result.caller_tag = mbx.request.caller_tag;
}

bn::optional<scene_kind> text_input_scene::update()
{
    bool need_redraw = false;

    if (bn::keypad::up_pressed())
    {
        grid_cursor_ = (grid_cursor_ + GRID_CELLS - GRID_COLS) % GRID_CELLS;
        need_redraw = true;
    }
    if (bn::keypad::down_pressed())
    {
        grid_cursor_ = (grid_cursor_ + GRID_COLS) % GRID_CELLS;
        need_redraw = true;
    }
    if (bn::keypad::left_pressed())
    {
        grid_cursor_ = (grid_cursor_ + GRID_CELLS - 1) % GRID_CELLS;
        need_redraw = true;
    }
    if (bn::keypad::right_pressed())
    {
        grid_cursor_ = (grid_cursor_ + 1) % GRID_CELLS;
        need_redraw = true;
    }

    if (bn::keypad::select_pressed())
    {
        cycle_tab();
        return bn::nullopt;
    }

    if (bn::keypad::a_pressed())
    {
        place_focused_char();
        return bn::nullopt;
    }

    if (bn::keypad::b_pressed())
    {
        backspace();
        // backspace() may have called cancel() and populated mbx.result.
        if (text_input_state().result.valid)
        {
            return return_to_;
        }
        return bn::nullopt;
    }

    if (bn::keypad::start_pressed())
    {
        commit();
        return return_to_;
    }

    if (need_redraw)
    {
        redraw();
    }
    return bn::nullopt;
}

}  // namespace pksave_gba
