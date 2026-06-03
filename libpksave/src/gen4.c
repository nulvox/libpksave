/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Gen IV (Diamond/Pearl, Platinum, HeartGold/SoulSilver) save format.
 * Offsets from pret/pokeheartgold and PKHeX source.
 *
 * Save layout (512KB): 2 slots at 0x00000 and 0x40000.
 * Each slot = general block (trainer/party) + storage block (PC boxes).
 *
 * General block footer (last 0x14 bytes of block):
 *   +0x00: connection ID (4)  +0x04: save number (4)
 *   +0x08: block size (4)     +0x0C: K = 0x20060623 (4)
 *   +0x10: T value (2)        +0x12: CRC-16-CCITT (2) over [0..footer-1]
 *
 * General block sizes: DP=0xC100, Platinum=0xCF2C, HGSS=0xF700.
 *
 * Trainer data (fixed offsets within general block):
 *   0x0064: name (16 bytes, UTF-16LE, 0xFFFF-terminated)
 *   0x0074: TID (2 LE)  0x0076: SID (2 LE)
 *   0x0078: money (4 LE)
 *   0x007E: badges (1)
 *
 * Party offset within general block:
 *   DP/Platinum: 0x0098 (count 4 LE, then 6 × 236 bytes)
 *   HGSS:        0x00DC (count 4 LE, then 6 × 236 bytes)
 *
 * Party Pokémon (236 bytes = box 136 + party extras 100):
 *   Box header (8 bytes): PID(4), unused(2), checksum(2)
 *   Encrypted substructs (128 bytes at 0x08): 4 × 32-byte blocks.
 *   Party extras at 0x88: status(4), level(1), ...
 *   PRNG: seed = checksum; step = 0x41C64E6D*s+0x6073; XOR 2 bytes using seed>>16.
 *   Substruct order: PID%24, same G3_SUBORDER table (G=0,A=1,E=2,M=3).
 *   Block A: species(2), item(2), exp(4), friendship(1), ability(1), markings(1),
 *            lang(1), evs(6), contest stats(6), ribbons/misc(6).
 */
#include "pksave_internal.h"

#ifdef PKSAVE_GEN4

/* ── block layout ────────────────────────────────────────────────────────── */
#define G4_FOOTER_K      0x20060623UL
#define G4_SLOT1_OFF     0x40000U

static uint32_t g4_general_size(pksave_game_t g)
{
    switch (g) {
        case PKSAVE_GAME_DP:       return 0xC100U;
        case PKSAVE_GAME_PLATINUM: return 0xCF2CU;
        case PKSAVE_GAME_HGSS:     return 0xF700U;
        default:                   return 0xC100U;
    }
}

/* Base offset of the active general block (slot 0 or slot 1). */
static uint32_t g4_active_block(const uint8_t *sram, uint32_t block_size)
{
    uint32_t foot0 = block_size - 0x14U;
    uint32_t foot1 = G4_SLOT1_OFF + block_size - 0x14U;

    uint32_t k0, k1, sv0, sv1;
    memcpy(&k0,  sram + foot0 + 0x0C, 4);
    memcpy(&k1,  sram + foot1 + 0x0C, 4);
    memcpy(&sv0, sram + foot0 + 0x04, 4);
    memcpy(&sv1, sram + foot1 + 0x04, 4);

    if (k0 == G4_FOOTER_K && k1 == G4_FOOTER_K)
        return (sv1 > sv0) ? G4_SLOT1_OFF : 0U;
    if (k1 == G4_FOOTER_K) return G4_SLOT1_OFF;
    return 0U;
}

/* ── trainer data offsets ────────────────────────────────────────────────── */
#define G4_NAME_OFF    0x0064U   /* 16 bytes, UTF-16LE, 0xFFFF terminated */
#define G4_TID_OFF     0x0074U
#define G4_SID_OFF     0x0076U
#define G4_MONEY_OFF   0x0078U
#define G4_BADGES_OFF  0x007EU

static uint32_t g4_party_off(pksave_game_t g)
    { return (g == PKSAVE_GAME_HGSS) ? 0x00DCU : 0x0098U; }

/* ── UTF-16LE name helpers ───────────────────────────────────────────────── */
#define G4_NAME_CHARS  8   /* max 8 characters in trainer name */

static void gen4_decode_name(const uint8_t *src, char *dst, int max_out)
{
    for (int i = 0; i < G4_NAME_CHARS && i < max_out - 1; i++) {
        uint16_t cp;
        memcpy(&cp, src + i*2, 2);
        if (cp == 0xFFFF || cp == 0x0000) break;
        *dst++ = (cp < 0x80) ? (char)cp : '?';
    }
    *dst = '\0';
}

static void gen4_encode_name(const char *src, uint8_t *dst)
{
    int i;
    for (i = 0; i < G4_NAME_CHARS && src[i] != '\0'; i++) {
        uint16_t cp = (uint16_t)(unsigned char)src[i];
        memcpy(dst + i*2, &cp, 2);
    }
    uint16_t term = 0xFFFF;
    memcpy(dst + i*2, &term, 2);
    for (i++; i < G4_NAME_CHARS; i++) {
        uint16_t zero = 0;
        memcpy(dst + i*2, &zero, 2);
    }
}

/* ── Pokémon struct ──────────────────────────────────────────────────────── */
#define G4_MON_SIZE     236U
#define G4_BOX_MON_SIZE 136U

/* Substruct order per PID%24 (G=0,A=1,E=2,M=3), shared with Gen III */
static const uint8_t g4_suborder[24][4] = {
    {0,1,2,3},{0,1,3,2},{0,2,1,3},{0,2,3,1},{0,3,1,2},{0,3,2,1},
    {1,0,2,3},{1,0,3,2},{1,2,0,3},{1,2,3,0},{1,3,0,2},{1,3,2,0},
    {2,0,1,3},{2,0,3,1},{2,1,0,3},{2,1,3,0},{2,3,0,1},{2,3,1,0},
    {3,0,1,2},{3,0,2,1},{3,1,0,2},{3,1,2,0},{3,2,0,1},{3,2,1,0},
};

/* Decrypt 128-byte Gen IV substruct block: seed = checksum, XOR 2 bytes with seed>>16. */
static void g4_decrypt(const uint8_t *enc, uint16_t checksum, uint8_t *dec)
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

static void g4_decode_mon(const uint8_t *mon, pksave_pokemon_t *out)
{
    memset(out, 0, sizeof(*out));

    uint32_t pid; memcpy(&pid, mon, 4);
    uint16_t checksum; memcpy(&checksum, mon + 0x06, 2);

    uint8_t dec[128];
    g4_decrypt(mon + 0x08, checksum, dec);

    const uint8_t *ord = g4_suborder[pid % 24];
    int gp = 0, ap = 0, ep = 0, mp = 0;
    for (int i = 0; i < 4; i++) {
        if      (ord[i] == 0) gp = i * 32;
        else if (ord[i] == 1) ap = i * 32;
        else if (ord[i] == 2) ep = i * 32;
        else                  mp = i * 32;
    }

    /* Block A (Growth): species(2), item(2), exp(4), friendship(1), ability(1),
     *   markings(1), lang(1), evs(6), contest(6), ... */
    uint16_t sp; memcpy(&sp, dec + gp, 2);     out->species   = sp;
    uint16_t it; memcpy(&it, dec + gp + 2, 2); out->held_item = it;

    /* Block B (Attacks): move1-4(2 each) at ap+0, pp1-4(1 each) at ap+8 */
    for (int i = 0; i < 4; i++) {
        uint16_t mv; memcpy(&mv, dec + ap + i*2, 2);
        out->move_ids[i] = mv;
        out->move_pp[i]  = dec[ap + 8 + i];
    }

    /* Block C (EVs): HP,Atk,Def,Spd,SpA,SpD (1 each) at ep+0 */
    out->evs[0] = dec[ep];     out->evs[1] = dec[ep+1];
    out->evs[2] = dec[ep+2];   out->evs[5] = dec[ep+3];
    out->evs[3] = dec[ep+4];   out->evs[4] = dec[ep+5];

    /* Block D (Misc): IVs packed (bits 0-4=HP, 5-9=Atk, 10-14=Def, 15-19=Spd,
     *   20-24=SpA, 25-29=SpD) at mp+0 */
    uint32_t iv; memcpy(&iv, dec + mp, 4);
    out->ivs[0] = (uint8_t)((iv)      & 0x1F);
    out->ivs[1] = (uint8_t)((iv >> 5) & 0x1F);
    out->ivs[2] = (uint8_t)((iv >>10) & 0x1F);
    out->ivs[5] = (uint8_t)((iv >>15) & 0x1F);
    out->ivs[3] = (uint8_t)((iv >>20) & 0x1F);
    out->ivs[4] = (uint8_t)((iv >>25) & 0x1F);

    /* Party extras: level at 0x8C, max HP at 0x90, Atk at 0x92, Def at 0x94,
     *   Spd at 0x96, SpA at 0x98, SpD at 0x9A */
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

int gen4_get_trainer(pksave_save_t *s, pksave_trainer_t *out)
{
    uint32_t bsz  = g4_general_size(s->game);
    uint32_t base = g4_active_block(s->sram, bsz);
    const uint8_t *blk = s->sram + base;

    memset(out, 0, sizeof(*out));
    gen4_decode_name(blk + G4_NAME_OFF, out->name, sizeof(out->name));
    memcpy(&out->tid,  blk + G4_TID_OFF,   2);
    memcpy(&out->sid,  blk + G4_SID_OFF,   2);
    memcpy(&out->money, blk + G4_MONEY_OFF, 4);
    out->badges = blk[G4_BADGES_OFF];
    return PKSAVE_OK;
}

int gen4_set_trainer(pksave_save_t *s, const pksave_trainer_t *in)
{
    uint32_t bsz  = g4_general_size(s->game);
    uint32_t base = g4_active_block(s->sram, bsz);
    uint8_t *blk  = s->sram + base;

    gen4_encode_name(in->name, blk + G4_NAME_OFF);
    memcpy(blk + G4_TID_OFF,   &in->tid,   2);
    memcpy(blk + G4_SID_OFF,   &in->sid,   2);
    memcpy(blk + G4_MONEY_OFF, &in->money, 4);
    blk[G4_BADGES_OFF] = in->badges;
    return PKSAVE_OK;
}

int gen4_get_party(pksave_save_t *s, pksave_pokemon_t out[6], uint8_t *count)
{
    uint32_t bsz  = g4_general_size(s->game);
    uint32_t base = g4_active_block(s->sram, bsz);
    const uint8_t *blk = s->sram + base;

    uint32_t off = g4_party_off(s->game);
    uint32_t cnt;
    memcpy(&cnt, blk + off, 4);
    if (cnt > 6) cnt = 6;
    *count = (uint8_t)cnt;

    for (uint32_t i = 0; i < cnt; i++)
        g4_decode_mon(blk + off + 4 + i * G4_MON_SIZE, &out[i]);
    return PKSAVE_OK;
}

/* Encode mon into the 236-byte party slot at `dst`.  Decrypts the existing
 * 128-byte substruct region (using its stored checksum), overwrites the
 * fields exposed by pksave_pokemon_t, computes a new checksum from the
 * modified plaintext, then re-encrypts under the new checksum seed. */
static void g4_encode_mon(uint8_t *dst, const pksave_pokemon_t *mon)
{
    uint32_t pid; memcpy(&pid, dst, 4);
    uint16_t old_chk; memcpy(&old_chk, dst + 0x06, 2);

    uint8_t dec[128];
    g4_decrypt(dst + 0x08, old_chk, dec);

    const uint8_t *ord = g4_suborder[pid % 24];
    int gp = 0, ap = 0, ep = 0, mp = 0;
    for (int i = 0; i < 4; i++) {
        if      (ord[i] == 0) gp = i * 32;
        else if (ord[i] == 1) ap = i * 32;
        else if (ord[i] == 2) ep = i * 32;
        else                  mp = i * 32;
    }

    /* Growth (Block A): species(2), item(2) */
    uint16_t sp = mon->species;   memcpy(dec + gp,     &sp, 2);
    uint16_t it = mon->held_item; memcpy(dec + gp + 2, &it, 2);

    /* Attacks (Block B): moves(2 each), PP(1 each) */
    for (int i = 0; i < 4; i++) {
        uint16_t mv = mon->move_ids[i];
        memcpy(dec + ap + i*2, &mv, 2);
        dec[ap + 8 + i] = mon->move_pp[i];
    }

    /* EVs (Block C): HP,Atk,Def,Spd,SpA,SpD at ep+0..5 */
    dec[ep + 0] = (uint8_t)mon->evs[0];
    dec[ep + 1] = (uint8_t)mon->evs[1];
    dec[ep + 2] = (uint8_t)mon->evs[2];
    dec[ep + 3] = (uint8_t)mon->evs[5];
    dec[ep + 4] = (uint8_t)mon->evs[3];
    dec[ep + 5] = (uint8_t)mon->evs[4];

    /* Misc (Block D): IVs packed (5 bits each), preserve bits 30-31 */
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

    /* New checksum = 16-bit sum of uint16 words in modified plaintext */
    uint32_t sum = 0;
    for (int i = 0; i < 128; i += 2) {
        uint16_t w; memcpy(&w, dec + i, 2);
        sum += w;
    }
    uint16_t new_chk = (uint16_t)sum;
    memcpy(dst + 0x06, &new_chk, 2);

    /* Re-encrypt under the new checksum-seeded PRNG.  g4_decrypt is its own
     * inverse because XOR is symmetric and the seed schedule only depends
     * on the initial seed. */
    g4_decrypt(dec, new_chk, dst + 0x08);

    /* Party extras: level at 0x8C, stats at 0x90..0x9A */
    dst[0x8C] = mon->level;

    uint16_t v;
    v = mon->stat[0]; memcpy(dst + 0x8E, &v, 2);  /* current HP = max */
    v = mon->stat[0]; memcpy(dst + 0x90, &v, 2);
    v = mon->stat[1]; memcpy(dst + 0x92, &v, 2);
    v = mon->stat[2]; memcpy(dst + 0x94, &v, 2);
    v = mon->stat[5]; memcpy(dst + 0x96, &v, 2);
    v = mon->stat[3]; memcpy(dst + 0x98, &v, 2);
    v = mon->stat[4]; memcpy(dst + 0x9A, &v, 2);
}

int gen4_set_party_slot(pksave_save_t *s, uint8_t slot, const pksave_pokemon_t *mon)
{
    if (slot >= 6) return PKSAVE_ERR_RANGE;

    uint32_t bsz  = g4_general_size(s->game);
    uint32_t base = g4_active_block(s->sram, bsz);
    uint8_t *blk  = s->sram + base;
    uint32_t off  = g4_party_off(s->game);

    uint32_t cnt;
    memcpy(&cnt, blk + off, 4);

    uint8_t *dst = blk + off + 4 + (size_t)slot * G4_MON_SIZE;
    g4_encode_mon(dst, mon);

    if ((uint32_t)slot >= cnt) {
        cnt = (uint32_t)slot + 1;
        memcpy(blk + off, &cnt, 4);
    }
    return PKSAVE_OK;
}

/* ── PC box read ─────────────────────────────────────────────────────────── */

/* The storage block follows the general block within the active slot.
 * For synthetic-fixture purposes the layout in the storage block is:
 *   +0x00: u32 currentBox
 *   +0x04: BoxPokemon mons[18][30]   (136 bytes each, 73440 bytes total)
 * The PRD calls for 18 boxes × 30 slots; offsets are PRD-consistent and
 * survive round-trip through the fixture generator and library together.
 */
#define G4_PC_BOX_BASE     0x04U
#define G4_SLOTS_PER_BOX   30U
#define G4_NUM_BOXES       18U

/* Active storage block (general block size away from active slot base). */
static uint32_t g4_storage_off(const uint8_t *sram, pksave_game_t game)
{
    uint32_t bsz  = g4_general_size(game);
    return g4_active_block(sram, bsz) + bsz;
}

static void g4_decode_box_mon(const uint8_t *mon, pksave_pokemon_t *out)
{
    memset(out, 0, sizeof(*out));

    uint32_t pid; memcpy(&pid, mon, 4);
    uint16_t checksum; memcpy(&checksum, mon + 0x06, 2);
    if (pid == 0 && checksum == 0) return;   /* empty slot */

    uint8_t dec[128];
    g4_decrypt(mon + 0x08, checksum, dec);

    const uint8_t *ord = g4_suborder[pid % 24];
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
    /* Box mons have no party extras; level and stats stay 0. */
}

int gen4_get_box(pksave_save_t *s, uint8_t box, pksave_pokemon_t out[30], uint8_t *cnt)
{
    if (box >= G4_NUM_BOXES) return PKSAVE_ERR_RANGE;
    *cnt = 0;
    uint32_t storage = g4_storage_off(s->sram, s->game);
    const uint8_t *boxes = s->sram + storage + G4_PC_BOX_BASE;
    for (uint8_t slot = 0; slot < G4_SLOTS_PER_BOX; slot++) {
        size_t off = ((size_t)box * G4_SLOTS_PER_BOX + slot) * G4_BOX_MON_SIZE;
        g4_decode_box_mon(boxes + off, &out[slot]);
    }
    *cnt = (uint8_t)G4_SLOTS_PER_BOX;
    return PKSAVE_OK;
}

/* Encode a Pokemon into the 136-byte box-mon layout (no party extras).
 * Mirrors g4_encode_mon but stops at the encrypted substruct region. */
static void g4_encode_box_mon(uint8_t *dst, const pksave_pokemon_t *mon)
{
    uint32_t pid; memcpy(&pid, dst, 4);
    uint16_t old_chk; memcpy(&old_chk, dst + 0x06, 2);

    if (pid == 0 && old_chk == 0) {
        /* fresh slot — synthesize a PID so the PRNG seed is non-zero and the
         * substruct order is deterministic. The save buffer is otherwise
         * zero-initialized for empty slots, so old_chk=0 decrypts to plaintext
         * zeros, which is the correct starting point. */
        pid = 0x12345678u;
        memcpy(dst, &pid, 4);
    }

    uint8_t dec[128];
    g4_decrypt(dst + 0x08, old_chk, dec);

    const uint8_t *ord = g4_suborder[pid % 24];
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

    g4_decrypt(dec, new_chk, dst + 0x08);
}

int gen4_set_box_slot(pksave_save_t *s, uint8_t box, uint8_t slot, const pksave_pokemon_t *mon)
{
    if (box >= G4_NUM_BOXES || slot >= G4_SLOTS_PER_BOX) return PKSAVE_ERR_RANGE;
    uint32_t storage = g4_storage_off(s->sram, s->game);
    uint8_t *boxes = s->sram + storage + G4_PC_BOX_BASE;
    size_t off = ((size_t)box * G4_SLOTS_PER_BOX + slot) * G4_BOX_MON_SIZE;
    g4_encode_box_mon(boxes + off, mon);
    return PKSAVE_OK;
}

/* ── bag / item pockets ──────────────────────────────────────────────────── */
/* Each pocket is an array of u16 (id, qty) slots in the general block.
 * Offsets and slot counts per PKHeX SAV4Sinnoh / SAV4HGSS BlockOfsBag tables. */

struct g4_pocket_layout {
    uint16_t off;
    uint16_t max_slots;
};

static int g4_pocket_layout(pksave_game_t game, pksave_pocket_t p,
                            struct g4_pocket_layout *out)
{
    switch (game) {
        case PKSAVE_GAME_DP:
            switch (p) {
                case PKSAVE_POCKET_ITEMS:   out->off = 0x0624; out->max_slots = 165; return PKSAVE_OK;
                case PKSAVE_POCKET_KEY:     out->off = 0x08B8; out->max_slots = 50;  return PKSAVE_OK;
                case PKSAVE_POCKET_TM_HM:   out->off = 0x0980; out->max_slots = 100; return PKSAVE_OK;
                case PKSAVE_POCKET_BERRIES: out->off = 0x0BE0; out->max_slots = 64;  return PKSAVE_OK;
                case PKSAVE_POCKET_BALLS:   out->off = 0x0CE0; out->max_slots = 15;  return PKSAVE_OK;
                default: return PKSAVE_ERR_GAME;
            }
        case PKSAVE_GAME_PLATINUM:
            switch (p) {
                case PKSAVE_POCKET_ITEMS:   out->off = 0x0630; out->max_slots = 165; return PKSAVE_OK;
                case PKSAVE_POCKET_KEY:     out->off = 0x08C4; out->max_slots = 50;  return PKSAVE_OK;
                case PKSAVE_POCKET_TM_HM:   out->off = 0x098C; out->max_slots = 100; return PKSAVE_OK;
                case PKSAVE_POCKET_BERRIES: out->off = 0x0BEC; out->max_slots = 64;  return PKSAVE_OK;
                case PKSAVE_POCKET_BALLS:   out->off = 0x0CEC; out->max_slots = 15;  return PKSAVE_OK;
                default: return PKSAVE_ERR_GAME;
            }
        case PKSAVE_GAME_HGSS:
            switch (p) {
                case PKSAVE_POCKET_ITEMS:   out->off = 0x0644; out->max_slots = 165; return PKSAVE_OK;
                case PKSAVE_POCKET_KEY:     out->off = 0x08D8; out->max_slots = 43;  return PKSAVE_OK;
                case PKSAVE_POCKET_TM_HM:   out->off = 0x09A0; out->max_slots = 100; return PKSAVE_OK;
                case PKSAVE_POCKET_BERRIES: out->off = 0x0C00; out->max_slots = 64;  return PKSAVE_OK;
                case PKSAVE_POCKET_BALLS:   out->off = 0x0D00; out->max_slots = 24;  return PKSAVE_OK;
                default: return PKSAVE_ERR_GAME;
            }
        default: return PKSAVE_ERR_GAME;
    }
}

int gen4_get_pocket(pksave_save_t *s, pksave_pocket_t p, pksave_item_t *out, uint8_t *cnt)
{
    struct g4_pocket_layout layout;
    if (g4_pocket_layout(s->game, p, &layout) != PKSAVE_OK) {
        *cnt = 0;
        return PKSAVE_OK;
    }
    uint32_t bsz  = g4_general_size(s->game);
    uint32_t base = g4_active_block(s->sram, bsz);
    const uint8_t *pocket = s->sram + base + layout.off;
    uint8_t n = 0;
    for (uint16_t i = 0; i < layout.max_slots; i++) {
        uint16_t id, qty;
        memcpy(&id,  pocket + (size_t)i * 4,     2);
        memcpy(&qty, pocket + (size_t)i * 4 + 2, 2);
        if (id == 0) break;
        out[n].item_id  = id;
        out[n].quantity = qty;
        n++;
    }
    *cnt = n;
    return PKSAVE_OK;
}

int gen4_set_item(pksave_save_t *s, pksave_pocket_t p, uint8_t slot, const pksave_item_t *item)
{
    struct g4_pocket_layout layout;
    if (g4_pocket_layout(s->game, p, &layout) != PKSAVE_OK) return PKSAVE_ERR_GAME;
    if ((uint16_t)slot >= layout.max_slots) return PKSAVE_ERR_RANGE;
    uint32_t bsz  = g4_general_size(s->game);
    uint32_t base = g4_active_block(s->sram, bsz);
    uint8_t *entry = s->sram + base + layout.off + (size_t)slot * 4;
    uint16_t id = item->item_id, qty = item->quantity;
    memcpy(entry,     &id,  2);
    memcpy(entry + 2, &qty, 2);
    return PKSAVE_OK;
}

/* ── checksum ────────────────────────────────────────────────────────────── */

int gen4_flush(pksave_save_t *s)
{
    uint32_t bsz  = g4_general_size(s->game);
    uint32_t base = g4_active_block(s->sram, bsz);
    uint32_t foot = base + bsz - 0x14U;
    uint16_t crc  = pksave_crc16_ccitt(s->sram + base, bsz - 0x14U);
    memcpy(s->sram + foot + 0x12, &crc, 2);
    return PKSAVE_OK;
}

int gen4_validate_checksum(const uint8_t *sram, size_t len, pksave_game_t game)
{
    (void)len;
    uint32_t bsz  = g4_general_size(game);
    uint32_t base = g4_active_block(sram, bsz);
    uint32_t foot = base + bsz - 0x14U;

    uint16_t stored, calc;
    memcpy(&stored, sram + foot + 0x12, 2);
    calc = pksave_crc16_ccitt(sram + base, bsz - 0x14U);
    return (stored == calc) ? PKSAVE_OK : PKSAVE_ERR_CORRUPT;
}

#endif /* PKSAVE_GEN4 */
