// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "numeric_field.hpp"

#include <bn_keypad.h>

namespace pksave_gba {
namespace {

// Initial-press delta is always 1 (or 10 for L/R) on frame 1.
// After that we wait `slow_repeat_interval` frames between repeats,
// then ramp.
constexpr int INITIAL_HOLD_DELAY = 10;   // frames before repeat kicks in
constexpr int SLOW_INTERVAL      = 10;   // ~6 Hz at 60 fps
constexpr int MEDIUM_THRESHOLD   = 40;
constexpr int MEDIUM_INTERVAL    = 4;    // ~15 Hz
constexpr int FAST_THRESHOLD     = 80;
constexpr int FAST_INTERVAL      = 2;    // ~30 Hz

}  // namespace

numeric_field::numeric_field(int initial, int min, int max)
    : value_(initial)
    , min_(min)
    , max_(max)
    , up_frames_(0)
    , down_frames_(0)
    , left_frames_(0)
    , right_frames_(0)
{
    set_value(initial);
}

void numeric_field::set_value(int v)
{
    if (v < min_) v = min_;
    if (v > max_) v = max_;
    value_ = v;
}

bool numeric_field::should_step_this_frame(int hold_frames) const
{
    if (hold_frames == 1) return true;                 // tap / first press
    if (hold_frames < INITIAL_HOLD_DELAY) return false; // dead zone

    int interval = SLOW_INTERVAL;
    if (hold_frames >= FAST_THRESHOLD)        interval = FAST_INTERVAL;
    else if (hold_frames >= MEDIUM_THRESHOLD) interval = MEDIUM_INTERVAL;

    // Count from when the repeat regime started.
    return ((hold_frames - INITIAL_HOLD_DELAY) % interval) == 0;
}

bool numeric_field::tick()
{
    // Increment hold counters on currently-held axes; reset others.
    up_frames_    = bn::keypad::up_held()    ? (up_frames_    + 1) : 0;
    down_frames_  = bn::keypad::down_held()  ? (down_frames_  + 1) : 0;
    left_frames_  = bn::keypad::left_held()  ? (left_frames_  + 1) : 0;
    right_frames_ = bn::keypad::right_held() ? (right_frames_ + 1) : 0;

    int delta = 0;
    if (up_frames_    && should_step_this_frame(up_frames_))    delta += 1;
    if (down_frames_  && should_step_this_frame(down_frames_))  delta -= 1;
    if (right_frames_ && should_step_this_frame(right_frames_)) delta += 10;
    if (left_frames_  && should_step_this_frame(left_frames_))  delta -= 10;

    if (delta == 0)
    {
        return false;
    }

    int prev = value_;
    set_value(value_ + delta);
    return value_ != prev;
}

}  // namespace pksave_gba
