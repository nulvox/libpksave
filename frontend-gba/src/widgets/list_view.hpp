// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Vertical scrollable list of fixed-height rows.  The widget is purely
// presentational: it owns sprites for the visible rows and a 1-line
// selection cursor, plus an int "selected" index it exposes to callers.
// It does not drill into anything itself — callers observe the selected
// index and react when the A button is pressed.
//
// Usage:
//   list_view lv(text_gen, /*row_count=*/N, /*viewport_rows=*/5,
//                /*x=*/-104, /*y_top=*/-40, /*row_h=*/16);
//   lv.set_row_provider([&](int i) -> bn::string<32> { ... });
//   lv.redraw();
//   ...
//   // per frame:
//   lv.tick();                  // reads keypad up/down, updates selection,
//                               // auto-scrolls + redraws if changed.
//   int focus = lv.selected();  // caller checks A_pressed for drill-down

#ifndef PKSAVE_GBA_WIDGETS_LIST_VIEW_HPP
#define PKSAVE_GBA_WIDGETS_LIST_VIEW_HPP

#include <bn_sprite_text_generator.h>
#include <bn_sprite_ptr.h>
#include <bn_string.h>
#include <bn_vector.h>

namespace pksave_gba {

class list_view {
public:
    // Row content is produced on demand by a callback.  Butano avoids
    // std::function, so we use a C-style function pointer plus an
    // opaque context pointer.  Callers pass a trampoline that casts
    // ctx back to the scene type.
    //
    // The callback writes its row text into the provided string and is
    // expected to be cheap (a few appends per call).
    using row_provider_fn = void (*)(void* ctx, int row, bn::string<32>& out);

    list_view(bn::sprite_text_generator& text_gen,
              int row_count,
              int viewport_rows,
              int x,
              int y_top,
              int row_h);

    // Replace the row provider and redraw.  Called by the owning scene
    // after constructing the list_view; the deferred set keeps the
    // ctor signature simple.
    void set_row_provider(row_provider_fn fn, void* ctx);

    // Read keypad up/down, update selection, auto-scroll, redraw if
    // anything visible changed.  Returns true iff selection changed
    // this frame — caller can use this to decide whether to re-evaluate
    // any derived state.
    bool tick();

    // Force a full redraw (e.g. when the owning scene flips a flag that
    // affects row text).
    void redraw();

    // Replace the row count (e.g. when the owning scene switches to a
    // different tab whose field list is shorter or empty).  Clamps
    // `selected_` and `top_row_` into the new range and redraws.
    void set_row_count(int row_count);

    int selected() const { return selected_; }
    int row_count() const { return row_count_; }

private:
    bn::sprite_text_generator&     text_gen_;
    bn::vector<bn::sprite_ptr, 32> sprites_;
    row_provider_fn                provider_fn_;
    void*                          provider_ctx_;
    int row_count_;
    int viewport_rows_;
    int x_;
    int y_top_;
    int row_h_;
    int selected_;
    int top_row_;   // index of the row drawn at viewport position 0
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_WIDGETS_LIST_VIEW_HPP
