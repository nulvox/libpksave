// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Gen-3-style text-input keyboard.  Modeled as a butano scene so the
// scene loop in main.cpp drives it without nested-update gymnastics:
// the trainer scene "opens" the keyboard by populating a static
// `text_input::request` slot and returning scene_kind::TEXT_INPUT;
// when the user commits or cancels, text_input_scene populates the
// result slot and returns to TRAINER.
//
// Keyboard grammar (per the plan §5 / §7):
//   - SELECT       : cycle tab (upper / lower / symbol)
//   - D-pad        : navigate the 5x6 grid (wraps)
//   - A            : place the focused character at the cursor (no-op
//                    when buffer is full)
//   - B            : delete the last character; if buffer empty, cancel
//                    (return to caller with cancelled=true)
//   - START        : commit (return to caller with cancelled=false)
//
// The character set comes from text_input_charset.h — same source
// libpksave's Unity round-trip test consumes, so the keyboard cannot
// expose anything libpksave can't encode.

#ifndef PKSAVE_GBA_WIDGETS_TEXT_INPUT_HPP
#define PKSAVE_GBA_WIDGETS_TEXT_INPUT_HPP

#include "scenes/scene.hpp"

#include <bn_sprite_text_generator.h>
#include <bn_sprite_ptr.h>
#include <bn_string.h>
#include <bn_vector.h>

namespace pksave_gba {

// Single-slot mailbox connecting trainer_scene <-> text_input_scene.
// The caller writes `request`, returns scene_kind::TEXT_INPUT, and on
// re-entry to the caller scene reads `result`.  Only one keyboard is
// active at a time so a single static slot is sufficient.
struct text_input_mailbox {
    struct request_t {
        bn::string<16> initial;
        int            max_len;
        // Caller hint to map "what scene returns when the keyboard
        // closes".  Set by the requester before returning TEXT_INPUT.
        scene_kind     return_to;
    };
    struct result_t {
        bn::string<16> value;     // committed or partial-at-cancel
        bool           cancelled;
        bool           valid;     // true after text_input_scene populates it
    };
    request_t request;
    result_t  result;
};

// Singleton mailbox.  Defined in text_input.cpp.
text_input_mailbox& text_input_state();

class text_input_scene final : public scene {
public:
    text_input_scene();

    bn::optional<scene_kind> update() override;

private:
    void redraw();
    void place_focused_char();
    void cycle_tab();
    void backspace();
    void commit();
    void cancel();

    bn::sprite_text_generator     text_gen_;
    bn::vector<bn::sprite_ptr, 64> sprites_;

    bn::string<16> buffer_;
    int            max_len_;
    scene_kind     return_to_;

    int tab_;          // 0=upper, 1=lower, 2=symbol
    int grid_cursor_;  // index into the current tab's character string
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_WIDGETS_TEXT_INPUT_HPP
