// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "list_view.hpp"

#include <bn_keypad.h>

namespace pksave_gba {

list_view::list_view(bn::sprite_text_generator& text_gen,
                     int row_count,
                     int viewport_rows,
                     int x,
                     int y_top,
                     int row_h)
    : text_gen_(text_gen)
    , provider_fn_(nullptr)
    , provider_ctx_(nullptr)
    , row_count_(row_count)
    , viewport_rows_(viewport_rows)
    , x_(x)
    , y_top_(y_top)
    , row_h_(row_h)
    , selected_(0)
    , top_row_(0)
{
    // No-provider redraw is a no-op until the caller installs one.  We
    // don't draw anything until set_row_provider() is called.
}

void list_view::set_row_provider(row_provider_fn fn, void* ctx)
{
    provider_fn_  = fn;
    provider_ctx_ = ctx;
    redraw();
}

void list_view::redraw()
{
    sprites_.clear();
    if (provider_fn_ == nullptr)
    {
        return;
    }

    const int last_visible = bn::min(top_row_ + viewport_rows_, row_count_);
    text_gen_.set_left_alignment();

    for (int row = top_row_; row < last_visible; ++row)
    {
        bn::string<32> body;
        provider_fn_(provider_ctx_, row, body);

        // Prepend "> " for selection, two spaces otherwise so the row's
        // body text stays at the same x.
        bn::string<36> line;
        line.append((row == selected_) ? "> " : "  ");
        line.append(body);

        const int y = y_top_ + (row - top_row_) * row_h_;
        text_gen_.generate(x_, y, line, sprites_);
    }
}

bool list_view::tick()
{
    if (row_count_ <= 0)
    {
        return false;
    }

    int prev = selected_;

    if (bn::keypad::up_pressed())
    {
        selected_ = (selected_ + row_count_ - 1) % row_count_;
    }
    if (bn::keypad::down_pressed())
    {
        selected_ = (selected_ + 1) % row_count_;
    }

    if (selected_ == prev)
    {
        return false;
    }

    // Auto-scroll: keep selection in viewport.  When wrapping past the
    // end (or beginning), snap the viewport to show the selection plus
    // as much trailing context as fits.
    if (selected_ < top_row_)
    {
        top_row_ = selected_;
    }
    else if (selected_ >= top_row_ + viewport_rows_)
    {
        top_row_ = selected_ - viewport_rows_ + 1;
    }
    // Clamp top_row_ to a valid window even after wrap.
    const int max_top = bn::max(0, row_count_ - viewport_rows_);
    if (top_row_ > max_top) top_row_ = max_top;
    if (top_row_ < 0) top_row_ = 0;

    redraw();
    return true;
}

}  // namespace pksave_gba
