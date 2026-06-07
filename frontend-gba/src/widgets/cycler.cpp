// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "cycler.hpp"

#include "keypad_input.hpp"

namespace pksave_gba {
namespace {

// Match numeric_field.cpp exactly so hold-to-fast feel is uniform.
constexpr int INITIAL_HOLD_DELAY = 10;
constexpr int SLOW_INTERVAL      = 10;
constexpr int MEDIUM_THRESHOLD   = 40;
constexpr int MEDIUM_INTERVAL    = 4;
constexpr int FAST_THRESHOLD     = 80;
constexpr int FAST_INTERVAL      = 2;

}  // namespace

cycler::cycler(int initial_id, int min_id, int max_id)
    : id_(initial_id)
    , min_id_(min_id)
    , max_id_(max_id)
    , filter_fn_(nullptr)
    , filter_ctx_(nullptr)
    , left_frames_(0)
    , right_frames_(0)
    , left_armed_(true)
    , right_armed_(true)
{
    set_id(initial_id);
}

void cycler::set_filter(filter_fn fn, void* ctx)
{
    filter_fn_  = fn;
    filter_ctx_ = ctx;
}

void cycler::set_id(int v)
{
    if (v < min_id_) v = min_id_;
    if (v > max_id_) v = max_id_;
    id_ = v;
}

void cycler::on_focus_acquired()
{
    left_armed_  = !keypad_input::left_held();
    right_armed_ = !keypad_input::right_held();
    left_frames_ = right_frames_ = 0;
}

bool cycler::should_step_this_frame(int hold_frames) const
{
    if (hold_frames == 1) return true;
    if (hold_frames < INITIAL_HOLD_DELAY) return false;

    int interval = SLOW_INTERVAL;
    if (hold_frames >= FAST_THRESHOLD)        interval = FAST_INTERVAL;
    else if (hold_frames >= MEDIUM_THRESHOLD) interval = MEDIUM_INTERVAL;

    return ((hold_frames - INITIAL_HOLD_DELAY) % interval) == 0;
}

bool cycler::step(int direction)
{
    int candidate = id_ + direction;
    while (candidate >= min_id_ && candidate <= max_id_) {
        if (filter_fn_ == nullptr || filter_fn_(filter_ctx_, candidate)) {
            id_ = candidate;
            return true;
        }
        candidate += direction;
    }
    return false;
}

bool cycler::tick()
{
    if (!keypad_input::left_held())  left_armed_  = true;
    if (!keypad_input::right_held()) right_armed_ = true;

    left_frames_  = (left_armed_  && keypad_input::left_held())  ? (left_frames_  + 1) : 0;
    right_frames_ = (right_armed_ && keypad_input::right_held()) ? (right_frames_ + 1) : 0;

    bool changed = false;
    if (right_frames_ && should_step_this_frame(right_frames_)) changed |= step(+1);
    if (left_frames_  && should_step_this_frame(left_frames_))  changed |= step(-1);
    return changed;
}

}  // namespace pksave_gba
