// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Host-side keypad input stub. The cycler unit test flips these
// booleans frame-by-frame to drive the widget under test.

#include "keypad_input.hpp"

namespace pksave_gba::keypad_input {

bool g_left_held  = false;
bool g_right_held = false;

bool left_held()  { return g_left_held; }
bool right_held() { return g_right_held; }

}  // namespace pksave_gba::keypad_input
