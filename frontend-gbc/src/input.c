/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 */
#include "input.h"

static uint8_t g_keys_held    = 0;
static uint8_t g_keys_pressed = 0;
static uint8_t g_keys_prev    = 0;

void input_poll(void)
{
    uint8_t cur = joypad();
    g_keys_held    = cur;
    g_keys_pressed = (uint8_t)(cur & (uint8_t)(~g_keys_prev));
    g_keys_prev    = cur;
}

uint8_t input_pressed(uint8_t mask)
{
    return (g_keys_pressed & mask) != 0U;
}

uint8_t input_held(uint8_t mask)
{
    return (g_keys_held & mask) != 0U;
}

/* ── Scrollable list ────────────────────────────────────────────────────── */

static void list_view_clamp_scroll(list_view_t *lv)
{
    if (lv->selected < lv->scroll_top) {
        lv->scroll_top = lv->selected;
    } else if (lv->selected >= (uint8_t)(lv->scroll_top + lv->viewport_rows)) {
        lv->scroll_top = (uint8_t)(lv->selected - lv->viewport_rows + 1U);
    }
}

void list_view_init(list_view_t *lv, uint8_t count, uint8_t viewport_rows)
{
    lv->selected      = 0U;
    lv->count         = count;
    lv->viewport_rows = viewport_rows;
    lv->scroll_top    = 0U;
}

void list_view_set_count(list_view_t *lv, uint8_t count)
{
    lv->count = count;
    if (count == 0U) {
        lv->selected   = 0U;
        lv->scroll_top = 0U;
        return;
    }
    if (lv->selected >= count) {
        lv->selected = (uint8_t)(count - 1U);
    }
    list_view_clamp_scroll(lv);
}

uint8_t list_view_tick(list_view_t *lv, uint8_t wrap)
{
    if (lv->count <= 1U) return 0U;

    uint8_t prev = lv->selected;
    if (input_pressed(J_UP)) {
        if (lv->selected > 0U) {
            lv->selected--;
        } else if (wrap) {
            lv->selected = (uint8_t)(lv->count - 1U);
        }
    } else if (input_pressed(J_DOWN)) {
        if ((uint8_t)(lv->selected + 1U) < lv->count) {
            lv->selected++;
        } else if (wrap) {
            lv->selected = 0U;
        }
    }

    if (lv->selected != prev) {
        list_view_clamp_scroll(lv);
        return 1U;
    }
    return 0U;
}

/* ── Shared hold-to-accelerate curve ────────────────────────────────────── */

static uint8_t should_step_this_frame(uint16_t held_frames)
{
    if (held_frames == 0U) return 0U;
    if (held_frames == 1U) return 1U;
    if (held_frames < 10U) return 0U;
    if (held_frames < 30U) return ((held_frames - 10U) % 4U) == 0U;
    return 1U;
}

static uint16_t advance_axis(uint8_t is_held, uint8_t *disarmed, uint16_t counter)
{
    if (!is_held) {
        *disarmed = 0U;
        return 0U;
    }
    if (*disarmed) {
        return 0U;
    }
    return (uint16_t)(counter + 1U);
}

/* ── Numeric field ──────────────────────────────────────────────────────── */

static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void numeric_field_init(numeric_field_t *nf, int32_t initial,
                        int32_t min_val, int32_t max_val)
{
    nf->min_val      = min_val;
    nf->max_val      = max_val;
    nf->value        = clamp_i32(initial, min_val, max_val);
    nf->hold_up      = 0U;
    nf->hold_down    = 0U;
    nf->hold_left    = 0U;
    nf->hold_right   = 0U;
    nf->disarm_up    = 0U;
    nf->disarm_down  = 0U;
    nf->disarm_left  = 0U;
    nf->disarm_right = 0U;
}

void numeric_field_set_value(numeric_field_t *nf, int32_t v)
{
    nf->value = clamp_i32(v, nf->min_val, nf->max_val);
}

void numeric_field_set_max(numeric_field_t *nf, int32_t max_val)
{
    if (max_val < nf->min_val) max_val = nf->min_val;
    nf->max_val = max_val;
    nf->value   = clamp_i32(nf->value, nf->min_val, nf->max_val);
}

void numeric_field_on_focus(numeric_field_t *nf)
{
    nf->hold_up      = 0U;
    nf->hold_down    = 0U;
    nf->hold_left    = 0U;
    nf->hold_right   = 0U;
    nf->disarm_up    = input_held(J_UP);
    nf->disarm_down  = input_held(J_DOWN);
    nf->disarm_left  = input_held(J_LEFT);
    nf->disarm_right = input_held(J_RIGHT);
}

uint8_t numeric_field_tick(numeric_field_t *nf)
{
    nf->hold_up    = advance_axis(input_held(J_UP),    &nf->disarm_up,    nf->hold_up);
    nf->hold_down  = advance_axis(input_held(J_DOWN),  &nf->disarm_down,  nf->hold_down);
    nf->hold_left  = advance_axis(input_held(J_LEFT),  &nf->disarm_left,  nf->hold_left);
    nf->hold_right = advance_axis(input_held(J_RIGHT), &nf->disarm_right, nf->hold_right);

    int32_t delta = 0;
    if (should_step_this_frame(nf->hold_up))    delta += 1;
    if (should_step_this_frame(nf->hold_down))  delta -= 1;
    if (should_step_this_frame(nf->hold_right)) delta += 10;
    if (should_step_this_frame(nf->hold_left))  delta -= 10;

    if (delta == 0) return 0U;

    int32_t next = clamp_i32(nf->value + delta, nf->min_val, nf->max_val);
    if (next == nf->value) return 0U;
    nf->value = next;
    return 1U;
}

/* ── Cycler ─────────────────────────────────────────────────────────────── */

void cycler_init(cycler_t *c, uint16_t initial_id,
                 uint16_t min_id, uint16_t max_id,
                 cycler_filter_fn filter)
{
    c->min_id       = min_id;
    c->max_id       = max_id;
    c->id           = initial_id;
    c->hold_left    = 0U;
    c->hold_right   = 0U;
    c->disarm_left  = 0U;
    c->disarm_right = 0U;
    c->filter       = filter;
    cycler_set_id(c, initial_id);
}

void cycler_set_id(cycler_t *c, uint16_t id)
{
    if (id < c->min_id) id = c->min_id;
    if (id > c->max_id) id = c->max_id;
    c->id = id;
}

void cycler_on_focus(cycler_t *c)
{
    c->hold_left    = 0U;
    c->hold_right   = 0U;
    c->disarm_left  = input_held(J_LEFT);
    c->disarm_right = input_held(J_RIGHT);
}

static uint16_t cycler_step_once(const cycler_t *c, int8_t dir)
{
    uint16_t span = (uint16_t)(c->max_id - c->min_id + 1U);
    uint16_t cur  = c->id;
    for (uint16_t i = 0; i < span; i++) {
        if (dir > 0) {
            cur = (cur >= c->max_id) ? c->min_id : (uint16_t)(cur + 1U);
        } else {
            cur = (cur <= c->min_id) ? c->max_id : (uint16_t)(cur - 1U);
        }
        if (!c->filter || c->filter(cur)) {
            return cur;
        }
    }
    return c->id;
}

uint8_t cycler_tick(cycler_t *c)
{
    c->hold_left  = advance_axis(input_held(J_LEFT),  &c->disarm_left,  c->hold_left);
    c->hold_right = advance_axis(input_held(J_RIGHT), &c->disarm_right, c->hold_right);

    int8_t dir = 0;
    if (should_step_this_frame(c->hold_right)) dir += 1;
    if (should_step_this_frame(c->hold_left))  dir -= 1;

    if (dir == 0) return 0U;

    uint16_t next = cycler_step_once(c, dir);
    if (next == c->id) return 0U;
    c->id = next;
    return 1U;
}
