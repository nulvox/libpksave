// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Host-side unit tests for widgets/cycler.  Compiled with PKSAVE_GBA_HOSTTEST
// so the widget pulls in our keypad stub instead of butano's bn_keypad.

#include "cycler.hpp"
#include "keypad_input.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Test-controllable keypad state defined in keypad_input_host.cpp.
namespace pksave_gba::keypad_input {
extern bool g_left_held;
extern bool g_right_held;
}  // namespace pksave_gba::keypad_input

namespace kp = pksave_gba::keypad_input;
using pksave_gba::cycler;

static int g_pass = 0;
static int g_fail = 0;
static const char* g_current_test = nullptr;

#define RUN_TEST(fn) do {                                       \
        g_current_test = #fn;                                   \
        kp::g_left_held = false;                                \
        kp::g_right_held = false;                               \
        std::printf("  - %s ... ", #fn);                        \
        fn();                                                   \
        std::printf("OK\n");                                    \
        ++g_pass;                                               \
    } while (0)

#define CHECK(cond) do {                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL\n    %s:%d: %s in %s\n",                  \
                        __FILE__, __LINE__, #cond, g_current_test);     \
            ++g_fail;                                                   \
            return;                                                     \
        }                                                               \
    } while (0)

#define CHECK_EQ(a, b) do {                                             \
        auto _x = (a); auto _y = (b);                                   \
        if (!(_x == _y)) {                                              \
            std::printf("FAIL\n    %s:%d: %s == %s "                    \
                        "(got %lld vs %lld) in %s\n",                   \
                        __FILE__, __LINE__, #a, #b,                     \
                        (long long)_x, (long long)_y, g_current_test);  \
            ++g_fail;                                                   \
            return;                                                     \
        }                                                               \
    } while (0)

// ─── Test 1: construction ────────────────────────────────────────────────────
static void test_construct_holds_initial_id() {
    cycler c(5, 0, 10);
    CHECK_EQ(c.id(), 5);
}

// ─── Test 2: RIGHT edge advances ─────────────────────────────────────────────
static void test_right_edge_advances_id() {
    cycler c(5, 0, 10);
    kp::g_right_held = true;
    bool changed = c.tick();
    CHECK(changed);
    CHECK_EQ(c.id(), 6);
}

// ─── Test 3: LEFT edge retreats ──────────────────────────────────────────────
static void test_left_edge_retreats_id() {
    cycler c(5, 0, 10);
    kp::g_left_held = true;
    bool changed = c.tick();
    CHECK(changed);
    CHECK_EQ(c.id(), 4);
}

// ─── Test 4: saturate at max ─────────────────────────────────────────────────
static void test_right_at_max_saturates() {
    cycler c(10, 0, 10);
    kp::g_right_held = true;
    bool changed = c.tick();
    CHECK(!changed);
    CHECK_EQ(c.id(), 10);
}

// ─── Test 5: saturate at min ─────────────────────────────────────────────────
static void test_left_at_min_saturates() {
    cycler c(0, 0, 10);
    kp::g_left_held = true;
    bool changed = c.tick();
    CHECK(!changed);
    CHECK_EQ(c.id(), 0);
}

// ─── Test 6: filter rejects an id; RIGHT skips over it ───────────────────────
static bool reject_one_filter(void* ctx, int id) {
    return id != *static_cast<int*>(ctx);
}

static void test_filter_rejects_one_skips() {
    cycler c(5, 0, 10);
    int rejected = 6;
    c.set_filter(reject_one_filter, &rejected);
    kp::g_right_held = true;
    bool changed = c.tick();
    CHECK(changed);
    CHECK_EQ(c.id(), 7);
}

// ─── Test 6b: filter rejects on the left; LEFT skips over it ─────────────────
static void test_filter_rejects_one_left_skips() {
    cycler c(5, 0, 10);
    int rejected = 4;
    c.set_filter(reject_one_filter, &rejected);
    kp::g_left_held = true;
    bool changed = c.tick();
    CHECK(changed);
    CHECK_EQ(c.id(), 3);
}

// ─── Test 7: filter accepts only current id; cycling is a no-op ──────────────
static bool only_current_filter(void* ctx, int id) {
    return id == *static_cast<int*>(ctx);
}

static void test_filter_accepts_only_current_no_op() {
    cycler c(5, 0, 10);
    int accepted = 5;
    c.set_filter(only_current_filter, &accepted);

    kp::g_right_held = true;
    CHECK(!c.tick());
    CHECK_EQ(c.id(), 5);

    kp::g_right_held = false;
    kp::g_left_held  = true;
    CHECK(!c.tick());
    CHECK_EQ(c.id(), 5);
}

// Count frames-to-next-step starting from a cold widget with RIGHT held.
// Each call drives a single frame; returns true on the steps.
static int steps_in_first_n_frames(cycler& c, int n) {
    int steps = 0;
    for (int i = 0; i < n; ++i) {
        if (c.tick()) ++steps;
    }
    return steps;
}

// ─── Test 9: hold-to-fast cycle matches numeric_field accel curve ────────────
// Numbers traced from numeric_field's should_step_this_frame:
//   - Frame 1 always steps
//   - Frames 2..9 are dead zone
//   - From INITIAL_HOLD_DELAY=10 we step every SLOW_INTERVAL=10 frames
//     (at frames 10, 20, 30) → 4 steps in first 30 frames
//   - From frame 40 onwards, MEDIUM_INTERVAL=4 (so 40,44,48,…)
//   - From frame 80 onwards, FAST_INTERVAL=2.
// We assert a couple of points on the curve rather than the whole shape,
// to avoid brittleness on tunable constants.
static void test_hold_to_fast_cycle_curve() {
    // First-30-frame budget. Range wide enough to never saturate.
    cycler c(0, 0, 1000);
    kp::g_right_held = true;
    // Steps within first 30 frames at slow regime: frame 1, 10, 20, 30 = 4.
    int s30 = steps_in_first_n_frames(c, 30);
    CHECK_EQ(s30, 4);

    // Run the same widget a fresh 50 frames (frames 31..80 of total hold).
    // Frames 31..39 are slow tail (no steps).  Frame 40 starts medium →
    // 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80 — but 80 is also the
    // FAST_THRESHOLD edge.  Skip exact tail count, just assert "more
    // than slow".
    int s50_more = steps_in_first_n_frames(c, 50);
    CHECK(s50_more > 5);   // way more than 5 — medium kicked in
}

// ─── Test 8: on_focus_acquired disarms a still-held axis ─────────────────────
static void test_on_focus_acquired_disarms_held_axis() {
    cycler c(5, 0, 10);
    // RIGHT was held when focus landed here (sim: list_view consumed
    // a navigation edge driven by some other axis but the user is
    // still mashing RIGHT). The disarm prevents tick() from reading
    // it as a fresh edge.
    kp::g_right_held = true;
    c.on_focus_acquired();

    // First tick with RIGHT still held: must be a no-op.
    CHECK(!c.tick());
    CHECK_EQ(c.id(), 5);

    // User releases RIGHT: axis re-arms.
    kp::g_right_held = false;
    CHECK(!c.tick());
    CHECK_EQ(c.id(), 5);

    // User presses RIGHT again: now it should count as a fresh edge.
    kp::g_right_held = true;
    CHECK(c.tick());
    CHECK_EQ(c.id(), 6);
}

int main() {
    std::printf("test_cycler:\n");
    RUN_TEST(test_construct_holds_initial_id);
    RUN_TEST(test_right_edge_advances_id);
    RUN_TEST(test_left_edge_retreats_id);
    RUN_TEST(test_right_at_max_saturates);
    RUN_TEST(test_left_at_min_saturates);
    RUN_TEST(test_filter_rejects_one_skips);
    RUN_TEST(test_filter_rejects_one_left_skips);
    RUN_TEST(test_filter_accepts_only_current_no_op);
    RUN_TEST(test_on_focus_acquired_disarms_held_axis);
    RUN_TEST(test_hold_to_fast_cycle_curve);
    std::printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
