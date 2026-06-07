// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Hold-to-accelerate numeric editor.
//
// D-pad grammar (per the plan §5):
//   - Up    : +1   (tap once; hold for accel)
//   - Down  : -1   (tap once; hold for accel)
//   - Left  : -10  (tap; hold for accel)
//   - Right : +10  (tap; hold for accel)
// All deltas saturate at [min, max].
//
// Accel curve (frame-counted):
//   - Frame 0 of a press        → single delta
//   - Held continuously for >=10 frames → repeat at ~6 Hz (every 10 frames)
//   - Held >= 40 frames                 → ramp to every 4 frames
//   - Held >= 80 frames                 → every 2 frames
// Numbers chosen by feel; tune in Checkpoint #3 if users complain.
//
// The widget owns no sprites — the caller renders.  Per the Checkpoint
// #2 simplification, the field is always "live" when focused: the
// trainer screen treats every up/down/left/right while a numeric row
// is selected as an edit (no separate active-edit mode for this CP).
// Checkpoint #3's cycler will introduce the active-edit toggle.

#ifndef PKSAVE_GBA_WIDGETS_NUMERIC_FIELD_HPP
#define PKSAVE_GBA_WIDGETS_NUMERIC_FIELD_HPP

#include <cstdint>

namespace pksave_gba {

class numeric_field {
public:
    numeric_field(int initial, int min, int max);

    // Read keypad held/pressed state and step the value accordingly.
    // Returns true iff the value changed this frame.  Caller is
    // responsible for redrawing.
    bool tick();

    // Call when focus arrives at this field via list-view navigation.
    // Any d-pad axis currently held (typically the one that just drove
    // the navigation edge) is disarmed: it will be ignored by tick()
    // until the user releases and re-presses it. Without this, the
    // navigation edge would be re-interpreted as a fresh "frame 1"
    // step and bump the value once on arrival.
    // Bug discovered by frontend-gba/tests/automation/test_tid_roundtrip.py.
    void on_focus_acquired();

    int  value() const { return value_; }
    void set_value(int v);

    int  min() const { return min_; }
    int  max() const { return max_; }

    // Reseat the upper bound and clamp the current value into the new
    // range.  Used by callers whose max depends on another field (e.g.
    // the MOVES tab's PP cap = floor(base_pp * 1.6), which changes when
    // the move id changes).
    void set_max(int new_max);

private:
    // Compute the step magnitude this frame for a given hold counter
    // (0 = not held; 1 = first frame held; etc).
    bool should_step_this_frame(int hold_frames) const;

    int value_;
    int min_;
    int max_;

    int up_frames_;
    int down_frames_;
    int left_frames_;
    int right_frames_;

    // Per-axis arming. An axis disarmed by on_focus_acquired() ignores
    // hold input until the user releases the key (axis re-arms on
    // observing a not-held frame inside tick()).
    bool up_armed_;
    bool down_armed_;
    bool left_armed_;
    bool right_armed_;
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_WIDGETS_NUMERIC_FIELD_HPP
