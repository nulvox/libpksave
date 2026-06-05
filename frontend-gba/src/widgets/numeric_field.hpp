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

    int  value() const { return value_; }
    void set_value(int v);

    int  min() const { return min_; }
    int  max() const { return max_; }

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
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_WIDGETS_NUMERIC_FIELD_HPP
