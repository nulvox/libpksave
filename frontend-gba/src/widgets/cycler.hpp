// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Generic name-cycler widget used by the Pokemon editor for species /
// move / item / ability / nature pickers.  The widget tracks a current
// integer id in [min_id, max_id] and steps it via D-pad LEFT/RIGHT.
//
// Pattern mirrors numeric_field exactly: owns no sprites, no text_gen.
// The hosting scene renders `◀ <lookup_name(id())> ▶` itself, since
// each call site already knows which pksave_*_name() function to call
// and which `game` value to pass.  Keeping the cycler render-free is
// also what makes host-side unit testing trivial (see
// frontend-gba/tests/host/test_cycler.cpp).
//
// D-pad grammar (per plan §5):
//   - LEFT   : previous id (hold = accel)
//   - RIGHT  : next id     (hold = accel)
//   IDs saturate at min/max — they do not wrap.  Cyclers are seeded
//   from a finite range (species, moves, …) and the user can always
//   cycle back manually.
//
// Filter predicate: optional.  When set, tick() skips ids for which the
// predicate returns false, advancing in the same direction until it
// finds an accepted id.  If nothing in [min_id, max_id] is accepted in
// that direction, id stays put (degenerate case).  Use case: the
// ability cycler restricted to a species' two abilities.
//
// on_focus_acquired() — same fix as numeric_field's: when a list_view
// edge lands focus on this cycler, the axis that drove the edge may
// still be held; disarm it so tick() doesn't see a phantom "frame 1"
// step on the very next frame.  See
// docs/frontend-gba-plan.md §"Pitfall — list_view consumes UP/DOWN
// edges".  L/R aren't list_view's nav axes, but the same arming logic
// applies for any scene where a held axis might be reinterpreted.

#ifndef PKSAVE_GBA_WIDGETS_CYCLER_HPP
#define PKSAVE_GBA_WIDGETS_CYCLER_HPP

namespace pksave_gba {

class cycler {
public:
    using filter_fn = bool (*)(void* ctx, int id);

    cycler(int initial_id, int min_id, int max_id);

    // Optional filter.  Pass (nullptr, nullptr) to clear.  When set, the
    // filter's verdict applies on tick() — set_id() bypasses the filter
    // (it's the caller's responsibility to seed a valid id).
    void set_filter(filter_fn fn, void* ctx);

    // Read keypad L/R and step.  Returns true iff id changed this frame.
    bool tick();

    // See header docblock — call whenever the owning scene transfers
    // focus onto this cycler.
    void on_focus_acquired();

    int  id() const     { return id_; }
    int  min_id() const { return min_id_; }
    int  max_id() const { return max_id_; }

    // Saturating set; bypasses filter (seeding helper).
    void set_id(int v);

private:
    bool should_step_this_frame(int hold_frames) const;

    // Advance id by ±1 through the filter; return true if id changed.
    bool step(int direction);

    int id_;
    int min_id_;
    int max_id_;

    filter_fn filter_fn_;
    void*     filter_ctx_;

    int  left_frames_;
    int  right_frames_;
    bool left_armed_;
    bool right_armed_;
};

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_WIDGETS_CYCLER_HPP
