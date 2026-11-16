/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 *
 * Joypad polling and reusable UI widgets (list_view, numeric_field, cycler).
 */
#ifndef PKSAVE_GBC_INPUT_H
#define PKSAVE_GBC_INPUT_H

#include <gb/gb.h>
#include <stdint.h>

void    input_poll(void);
uint8_t input_pressed(uint8_t mask);
uint8_t input_held(uint8_t mask);

/* ── Scrollable list state ──────────────────────────────────────────────── */
typedef struct {
    uint8_t selected;
    uint8_t count;
    uint8_t viewport_rows;
    uint8_t scroll_top;
} list_view_t;

void    list_view_init(list_view_t *lv, uint8_t count, uint8_t viewport_rows);
void    list_view_set_count(list_view_t *lv, uint8_t count);
uint8_t list_view_tick(list_view_t *lv, uint8_t wrap);

/* ── Numeric field widget (hold-to-accelerate ±1 U/D, ±10 L/R) ──────────── */
typedef struct {
    int32_t  value;
    int32_t  min_val;
    int32_t  max_val;
    uint16_t hold_up;
    uint16_t hold_down;
    uint16_t hold_left;
    uint16_t hold_right;
    uint8_t  disarm_up;
    uint8_t  disarm_down;
    uint8_t  disarm_left;
    uint8_t  disarm_right;
} numeric_field_t;

void    numeric_field_init(numeric_field_t *nf, int32_t initial,
                           int32_t min_val, int32_t max_val);
void    numeric_field_set_value(numeric_field_t *nf, int32_t v);
void    numeric_field_set_max(numeric_field_t *nf, int32_t max_val);
void    numeric_field_on_focus(numeric_field_t *nf);
uint8_t numeric_field_tick(numeric_field_t *nf);

/* ── ID cycler widget (hold-to-accelerate L/R with optional filter) ─────── */
typedef uint8_t (*cycler_filter_fn)(uint16_t id);

typedef struct {
    uint16_t         id;
    uint16_t         min_id;
    uint16_t         max_id;
    uint16_t         hold_left;
    uint16_t         hold_right;
    uint8_t          disarm_left;
    uint8_t          disarm_right;
    cycler_filter_fn filter;
} cycler_t;

void    cycler_init(cycler_t *c, uint16_t initial_id,
                    uint16_t min_id, uint16_t max_id,
                    cycler_filter_fn filter);
void    cycler_set_id(cycler_t *c, uint16_t id);
void    cycler_on_focus(cycler_t *c);
uint8_t cycler_tick(cycler_t *c);

#endif /* PKSAVE_GBC_INPUT_H */
