/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Gen V (Black/White, Black 2/White 2) save format.
 * Same block/CRC structure as Gen IV; different block sizes and trainer offsets.
 *
 * General block sizes: BW=0x26A00, BW2=0x26E00.
 * Two slots at 0x00000 and 0x40000 (same as Gen IV).
 *
 * Trainer data within general block:
 *   0x19D0: name  (16 bytes, UTF-16LE, 0xFFFF-terminated)
 *   0x19E0: TID   (2 LE)
 *   0x19E2: SID   (2 LE)
 *   0x19E4: money (4 LE)
 *   0x19E8: badges (1)
 *
 * Party within general block:
 *   0x0200: team count (4 LE)
 *   0x0204: team data (6 × 236 bytes)
 *
 * Pokémon struct: same 236-byte party format as Gen IV (checksum-keyed PRNG,
 * 128-byte encrypted substructs, 32-byte blocks, PID%24 substruct ordering).
 */
#include "pksave_internal.h"

#ifdef PKSAVE_GEN5

/* ── block layout ────────────────────────────────────────────────────────── */
#define G5_FOOTER_K  0x20060623UL
#define G5_SLOT1_OFF 0x40000U

static uint32_t g5_general_size(pksave_game_t g)
{
    return (g == PKSAVE_GAME_BW2) ? 0x26E00U : 0x26A00U;
}

static uint32_t g5_active_block(const uint8_t *sram, uint32_t block_size)
{
    uint32_t foot0 = block_size - 0x14U;
    uint32_t foot1 = G5_SLOT1_OFF + block_size - 0x14U;

    uint32_t k0, k1, sv0, sv1;
    memcpy(&k0,  sram + foot0 + 0x0C, 4);
    memcpy(&k1,  sram + foot1 + 0x0C, 4);
    memcpy(&sv0, sram + foot0 + 0x04, 4);
    memcpy(&sv1, sram + foot1 + 0x04, 4);

    if (k0 == G5_FOOTER_K && k1 == G5_FOOTER_K)
        return (sv1 > sv0) ? G5_SLOT1_OFF : 0U;
    if (k1 == G5_FOOTER_K) return G5_SLOT1_OFF;
    return 0U;
}

/* ── trainer data offsets ────────────────────────────────────────────────── */
#define G5_TRAINER_OFF  0x19D0U
#define G5_NAME_OFF    (G5_TRAINER_OFF + 0U)   /* 16 bytes UTF-16LE */
#define G5_TID_OFF     (G5_TRAINER_OFF + 16U)
#define G5_SID_OFF     (G5_TRAINER_OFF + 18U)
#define G5_MONEY_OFF   (G5_TRAINER_OFF + 20U)
#define G5_BADGES_OFF  (G5_TRAINER_OFF + 24U)

#define G5_PARTY_CNT_OFF  0x0200U
#define G5_PARTY_DATA_OFF 0x0204U

/* ── UTF-16LE name helpers ───────────────────────────────────────────────── */
#define G5_NAME_CHARS  8

static void gen5_decode_name(const uint8_t *src, char *dst, int max_out)
{
    for (int i = 0; i < G5_NAME_CHARS && i < max_out - 1; i++) {
        uint16_t cp;
        memcpy(&cp, src + i*2, 2);
        if (cp == 0xFFFF || cp == 0x0000) break;
        *dst++ = (cp < 0x80) ? (char)cp : '?';
    }
    *dst = '\0';
}

static void gen5_encode_name(const char *src, uint8_t *dst)
{
    int i;
    for (i = 0; i < G5_NAME_CHARS && src[i] != '\0'; i++) {
        uint16_t cp = (uint16_t)(unsigned char)src[i];
        memcpy(dst + i*2, &cp, 2);
    }
    uint16_t term = 0xFFFF;
    memcpy(dst + i*2, &term, 2);
    for (i++; i < G5_NAME_CHARS; i++) {
        uint16_t zero = 0;
        memcpy(dst + i*2, &zero, 2);
    }
}

/* ── Pokémon struct (same as Gen IV) ─────────────────────────────────────── */
#define G5_MON_SIZE  236U

static const uint8_t g5_suborder[24][4] = {
    {0,1,2,3},{0,1,3,2},{0,2,1,3},{0,2,3,1},{0,3,1,2},{0,3,2,1},
    {1,0,2,3},{1,0,3,2},{1,2,0,3},{1,2,3,0},{1,3,0,2},{1,3,2,0},
    {2,0,1,3},{2,0,3,1},{2,1,0,3},{2,1,3,0},{2,3,0,1},{2,3,1,0},
    {3,0,1,2},{3,0,2,1},{3,1,0,2},{3,1,2,0},{3,2,0,1},{3,2,1,0},
};

static void g5_decrypt(const uint8_t *enc, uint16_t checksum, uint8_t *dec)
{
    uint32_t seed = checksum;
    for (int i = 0; i < 128; i += 2) {
        seed = 0x41C64E6Du * seed + 0x6073u;
        uint16_t mask = (uint16_t)(seed >> 16);
        uint16_t w;
        memcpy(&w, enc + i, 2);
        w ^= mask;
        memcpy(dec + i, &w, 2);
    }
}

static void g5_decode_mon(const uint8_t *mon, pksave_pokemon_t *out)
{
    memset(out, 0, sizeof(*out));

    uint32_t pid; memcpy(&pid, mon, 4);
    uint16_t checksum; memcpy(&checksum, mon + 0x06, 2);

    uint8_t dec[128];
    g5_decrypt(mon + 0x08, checksum, dec);

    const uint8_t *ord = g5_suborder[pid % 24];
    int gp = 0, ap = 0, ep = 0, mp = 0;
    for (int i = 0; i < 4; i++) {
        if      (ord[i] == 0) gp = i * 32;
        else if (ord[i] == 1) ap = i * 32;
        else if (ord[i] == 2) ep = i * 32;
        else                  mp = i * 32;
    }

    uint16_t sp; memcpy(&sp, dec + gp, 2);     out->species   = sp;
    uint16_t it; memcpy(&it, dec + gp + 2, 2); out->held_item = it;

    for (int i = 0; i < 4; i++) {
        uint16_t mv; memcpy(&mv, dec + ap + i*2, 2);
        out->move_ids[i] = mv;
        out->move_pp[i]  = dec[ap + 8 + i];
    }

    out->evs[0] = dec[ep];     out->evs[1] = dec[ep+1];
    out->evs[2] = dec[ep+2];   out->evs[5] = dec[ep+3];
    out->evs[3] = dec[ep+4];   out->evs[4] = dec[ep+5];

    uint32_t iv; memcpy(&iv, dec + mp, 4);
    out->ivs[0] = (uint8_t)((iv)      & 0x1F);
    out->ivs[1] = (uint8_t)((iv >> 5) & 0x1F);
    out->ivs[2] = (uint8_t)((iv >>10) & 0x1F);
    out->ivs[5] = (uint8_t)((iv >>15) & 0x1F);
    out->ivs[3] = (uint8_t)((iv >>20) & 0x1F);
    out->ivs[4] = (uint8_t)((iv >>25) & 0x1F);

    out->level = mon[0x8C];

    uint16_t v;
    memcpy(&v, mon + 0x90, 2); out->stat[0] = v;
    memcpy(&v, mon + 0x92, 2); out->stat[1] = v;
    memcpy(&v, mon + 0x94, 2); out->stat[2] = v;
    memcpy(&v, mon + 0x96, 2); out->stat[5] = v;
    memcpy(&v, mon + 0x98, 2); out->stat[3] = v;
    memcpy(&v, mon + 0x9A, 2); out->stat[4] = v;
}

/* ── public genN API ─────────────────────────────────────────────────────── */

int gen5_get_trainer(pksave_save_t *s, pksave_trainer_t *out)
{
    uint32_t bsz  = g5_general_size(s->game);
    uint32_t base = g5_active_block(s->sram, bsz);
    const uint8_t *blk = s->sram + base;

    memset(out, 0, sizeof(*out));
    gen5_decode_name(blk + G5_NAME_OFF, out->name, sizeof(out->name));
    memcpy(&out->tid,   blk + G5_TID_OFF,   2);
    memcpy(&out->sid,   blk + G5_SID_OFF,   2);
    memcpy(&out->money, blk + G5_MONEY_OFF, 4);
    out->badges = blk[G5_BADGES_OFF];
    return PKSAVE_OK;
}

int gen5_set_trainer(pksave_save_t *s, const pksave_trainer_t *in)
{
    uint32_t bsz  = g5_general_size(s->game);
    uint32_t base = g5_active_block(s->sram, bsz);
    uint8_t *blk  = s->sram + base;

    gen5_encode_name(in->name, blk + G5_NAME_OFF);
    memcpy(blk + G5_TID_OFF,   &in->tid,   2);
    memcpy(blk + G5_SID_OFF,   &in->sid,   2);
    memcpy(blk + G5_MONEY_OFF, &in->money, 4);
    blk[G5_BADGES_OFF] = in->badges;
    return PKSAVE_OK;
}

int gen5_get_party(pksave_save_t *s, pksave_pokemon_t out[6], uint8_t *count)
{
    uint32_t bsz  = g5_general_size(s->game);
    uint32_t base = g5_active_block(s->sram, bsz);
    const uint8_t *blk = s->sram + base;

    uint32_t cnt;
    memcpy(&cnt, blk + G5_PARTY_CNT_OFF, 4);
    if (cnt > 6) cnt = 6;
    *count = (uint8_t)cnt;

    for (uint32_t i = 0; i < cnt; i++)
        g5_decode_mon(blk + G5_PARTY_DATA_OFF + i * G5_MON_SIZE, &out[i]);
    return PKSAVE_OK;
}

/* Encode mon into the 236-byte party slot at `dst`.  Algorithm matches Gen IV
 * (checksum-seeded PRNG over 128 bytes of 4×32-byte substructs); only the
 * containing block base offset differs. */
static void g5_encode_mon(uint8_t *dst, const pksave_pokemon_t *mon)
{
    uint32_t pid; memcpy(&pid, dst, 4);
    uint16_t old_chk; memcpy(&old_chk, dst + 0x06, 2);

    uint8_t dec[128];
    g5_decrypt(dst + 0x08, old_chk, dec);

    const uint8_t *ord = g5_suborder[pid % 24];
    int gp = 0, ap = 0, ep = 0, mp = 0;
    for (int i = 0; i < 4; i++) {
        if      (ord[i] == 0) gp = i * 32;
        else if (ord[i] == 1) ap = i * 32;
        else if (ord[i] == 2) ep = i * 32;
        else                  mp = i * 32;
    }

    uint16_t sp = mon->species;   memcpy(dec + gp,     &sp, 2);
    uint16_t it = mon->held_item; memcpy(dec + gp + 2, &it, 2);

    for (int i = 0; i < 4; i++) {
        uint16_t mv = mon->move_ids[i];
        memcpy(dec + ap + i*2, &mv, 2);
        dec[ap + 8 + i] = mon->move_pp[i];
    }

    dec[ep + 0] = (uint8_t)mon->evs[0];
    dec[ep + 1] = (uint8_t)mon->evs[1];
    dec[ep + 2] = (uint8_t)mon->evs[2];
    dec[ep + 3] = (uint8_t)mon->evs[5];
    dec[ep + 4] = (uint8_t)mon->evs[3];
    dec[ep + 5] = (uint8_t)mon->evs[4];

    uint32_t iv;
    memcpy(&iv, dec + mp, 4);
    uint32_t preserved = iv & 0xC0000000u;
    iv = preserved
       | ((uint32_t)(mon->ivs[0] & 0x1F))
       | ((uint32_t)(mon->ivs[1] & 0x1F) << 5)
       | ((uint32_t)(mon->ivs[2] & 0x1F) << 10)
       | ((uint32_t)(mon->ivs[5] & 0x1F) << 15)
       | ((uint32_t)(mon->ivs[3] & 0x1F) << 20)
       | ((uint32_t)(mon->ivs[4] & 0x1F) << 25);
    memcpy(dec + mp, &iv, 4);

    uint32_t sum = 0;
    for (int i = 0; i < 128; i += 2) {
        uint16_t w; memcpy(&w, dec + i, 2);
        sum += w;
    }
    uint16_t new_chk = (uint16_t)sum;
    memcpy(dst + 0x06, &new_chk, 2);

    g5_decrypt(dec, new_chk, dst + 0x08);

    dst[0x8C] = mon->level;

    uint16_t v;
    v = mon->stat[0]; memcpy(dst + 0x8E, &v, 2);
    v = mon->stat[0]; memcpy(dst + 0x90, &v, 2);
    v = mon->stat[1]; memcpy(dst + 0x92, &v, 2);
    v = mon->stat[2]; memcpy(dst + 0x94, &v, 2);
    v = mon->stat[5]; memcpy(dst + 0x96, &v, 2);
    v = mon->stat[3]; memcpy(dst + 0x98, &v, 2);
    v = mon->stat[4]; memcpy(dst + 0x9A, &v, 2);
}

int gen5_set_party_slot(pksave_save_t *s, uint8_t slot, const pksave_pokemon_t *mon)
{
    if (slot >= 6) return PKSAVE_ERR_RANGE;

    uint32_t bsz  = g5_general_size(s->game);
    uint32_t base = g5_active_block(s->sram, bsz);
    uint8_t *blk  = s->sram + base;

    uint32_t cnt;
    memcpy(&cnt, blk + G5_PARTY_CNT_OFF, 4);

    uint8_t *dst = blk + G5_PARTY_DATA_OFF + (size_t)slot * G5_MON_SIZE;
    g5_encode_mon(dst, mon);

    if ((uint32_t)slot >= cnt) {
        cnt = (uint32_t)slot + 1;
        memcpy(blk + G5_PARTY_CNT_OFF, &cnt, 4);
    }
    return PKSAVE_OK;
}

int gen5_get_box(pksave_save_t *s, uint8_t box, pksave_pokemon_t out[30], uint8_t *cnt)
    { (void)s; (void)box; (void)out; *cnt = 0; return PKSAVE_OK; }

int gen5_set_box_slot(pksave_save_t *s, uint8_t box, uint8_t slot, const pksave_pokemon_t *mon)
    { (void)s; (void)box; (void)slot; (void)mon; return PKSAVE_ERR_RANGE; }

int gen5_get_pocket(pksave_save_t *s, pksave_pocket_t p, pksave_item_t *out, uint8_t *cnt)
    { (void)s; (void)p; (void)out; *cnt = 0; return PKSAVE_OK; }

int gen5_set_item(pksave_save_t *s, pksave_pocket_t p, uint8_t slot, const pksave_item_t *item)
    { (void)s; (void)p; (void)slot; (void)item; return PKSAVE_ERR_RANGE; }

/* ── checksum ────────────────────────────────────────────────────────────── */

int gen5_flush(pksave_save_t *s)
{
    uint32_t bsz  = g5_general_size(s->game);
    uint32_t base = g5_active_block(s->sram, bsz);
    uint32_t foot = base + bsz - 0x14U;
    uint16_t crc  = pksave_crc16_ccitt(s->sram + base, bsz - 0x14U);
    memcpy(s->sram + foot + 0x12, &crc, 2);
    return PKSAVE_OK;
}

int gen5_validate_checksum(const uint8_t *sram, size_t len, pksave_game_t game)
{
    (void)len;
    uint32_t bsz  = g5_general_size(game);
    uint32_t base = g5_active_block(sram, bsz);
    uint32_t foot = base + bsz - 0x14U;

    uint16_t stored, calc;
    memcpy(&stored, sram + foot + 0x12, 2);
    calc = pksave_crc16_ccitt(sram + base, bsz - 0x14U);
    return (stored == calc) ? PKSAVE_OK : PKSAVE_ERR_CORRUPT;
}

#endif /* PKSAVE_GEN5 */
