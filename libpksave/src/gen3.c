/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Gen III (Ruby/Sapphire, Emerald, FireRed/LeafGreen) save format.
 * Offsets from pret/pokeemerald and pret/pokeruby.
 *
 * Save layout (128KB): 2 slots × 14 sections × 4096 bytes.
 * Section 0 (SaveBlock2): trainer name(7), gender(1), TID(2 LE), SID(2 LE),
 *   badges(1 at 0x083), game_code/security_key(4 at 0x00AC), FRLG key(4 at 0x0AF8).
 * Section 1 (SaveBlock1) RS/E: team_cnt(4 at 0x234), team(6×100 at 0x238),
 *   money(4 at 0x490, XOR security key).
 * Section 1 FRLG: team_cnt(4 at 0x034), team(6×100 at 0x038), money(4 at 0x290).
 * Section footer (per section, 4096-byte boundary):
 *   0xFF4: section ID (2 LE), 0xFF6: checksum (2 LE),
 *   0xFF8: signature 0x08012025 (4), 0xFFC: save count (4 LE).
 * Party mon (100 bytes): box(80) + extras(20).
 *   Box: PID(4), OT_TID(2), OT_SID(2), nickname(10), lang(1), flags(1),
 *        OT_name(7), _pad(1), checksum(2), _pad2(2), encrypted_substructs(48).
 *   Extras at 0x50: status(2), level(1), mail(1), currentHP(2), maxHP(2),
 *        Atk(2), Def(2), Spd(2), SpA(2), SpD(2).
 * Substruct order: PID%24 indexes G3_SUBORDER (G=0,A=1,E=2,M=3), 12 bytes each.
 * PRNG: seed = PID^(OT_TID|(OT_SID<<16)); step = 0x41C64E6D*s+0x6073.
 */
#include "pksave_internal.h"

#ifdef PKSAVE_GEN3

/* ── section constants ───────────────────────────────────────────────────── */
#define G3_SECTION_SIZE  0x1000U
#define G3_NUM_SECTIONS  14U
#define G3_SIG           0x08012025UL
#define G3_SIG_OFF       0x0FF8U
#define G3_ID_OFF        0x0FF4U
#define G3_CHKSUM_OFF    0x0FF6U
#define G3_COUNT_OFF     0x0FFCU

static const uint16_t g3_sec_sizes[14] = {
    3884, 3968, 3968, 3968, 3848, 3968, 3968,
    3968, 3968, 3968, 3968, 3968, 3968, 2000
};

/* ── Gen III charset ─────────────────────────────────────────────────────── */

static char gen3_decode_char(uint8_t b)
{
    if (b == 0xFF) return '\0';
    if (b == 0x00) return ' ';
    if (b >= 0xA1 && b <= 0xBA) return (char)('A' + (b - 0xA1));
    if (b >= 0xBB && b <= 0xD4) return (char)('a' + (b - 0xBB));
    return '?';
}

static uint8_t gen3_encode_char(char c)
{
    if (c == '\0') return 0xFF;
    if (c == ' ')  return 0x00;
    if (c >= 'A' && c <= 'Z') return (uint8_t)(0xA1 + (c - 'A'));
    if (c >= 'a' && c <= 'z') return (uint8_t)(0xBB + (c - 'a'));
    return 0xFF;
}

static void gen3_decode_name(const uint8_t *src, char *dst, int max_out, int src_len)
{
    int i;
    for (i = 0; i < src_len && i < max_out - 1; i++) {
        if (src[i] == 0xFF) break;
        char c = gen3_decode_char(src[i]);
        if (c == '\0') break;
        *dst++ = c;
    }
    *dst = '\0';
}

static void gen3_encode_name(const char *src, uint8_t *dst, int dst_len)
{
    int i;
    for (i = 0; i < dst_len && src[i] != '\0'; i++)
        dst[i] = gen3_encode_char(src[i]);
    for (; i < dst_len; i++)
        dst[i] = 0xFF;
}

/* ── section utilities ───────────────────────────────────────────────────── */

static uint16_t g3_section_chksum(const uint8_t *sec, uint16_t id)
{
    uint16_t size = g3_sec_sizes[id < G3_NUM_SECTIONS ? id : 1];
    uint32_t sum = 0;
    for (uint16_t i = 0; (uint16_t)(i + 4U) <= size; i = (uint16_t)(i + 4)) {
        uint32_t w;
        memcpy(&w, sec + i, 4);
        sum += w;
    }
    return (uint16_t)((sum >> 16) + (sum & 0xFFFF));
}

static void g3_update_chksum(uint8_t *sec)
{
    uint16_t id;
    memcpy(&id, sec + G3_ID_OFF, 2);
    uint16_t chk = g3_section_chksum(sec, id);
    memcpy(sec + G3_CHKSUM_OFF, &chk, 2);
}

static uint32_t g3_slot_count(const uint8_t *sram, uint32_t slot_off)
{
    for (uint32_t s = 0; s < G3_NUM_SECTIONS; s++) {
        size_t off = slot_off + (size_t)s * G3_SECTION_SIZE;
        uint32_t sig;
        memcpy(&sig, sram + off + G3_SIG_OFF, 4);
        if (sig == G3_SIG) {
            uint32_t cnt;
            memcpy(&cnt, sram + off + G3_COUNT_OFF, 4);
            return cnt;
        }
    }
    return 0;
}

static uint32_t g3_active_slot(const uint8_t *sram)
{
    uint32_t cnt0 = g3_slot_count(sram, 0);
    uint32_t cnt1 = g3_slot_count(sram, G3_NUM_SECTIONS * G3_SECTION_SIZE);
    return (cnt1 > cnt0) ? (G3_NUM_SECTIONS * G3_SECTION_SIZE) : 0U;
}

static size_t g3_find_section(const uint8_t *sram, uint32_t slot_off, uint16_t target)
{
    for (uint32_t s = 0; s < G3_NUM_SECTIONS; s++) {
        size_t off = slot_off + (size_t)s * G3_SECTION_SIZE;
        uint32_t sig;
        memcpy(&sig, sram + off + G3_SIG_OFF, 4);
        if (sig != G3_SIG) continue;
        uint16_t id;
        memcpy(&id, sram + off + G3_ID_OFF, 2);
        if (id == target) return off;
    }
    return SIZE_MAX;
}

/* ── security key and money offsets ─────────────────────────────────────── */

static uint32_t g3_security_key(const uint8_t *sec0, pksave_game_t game)
{
    uint32_t key;
    if (game == PKSAVE_GAME_FRLG)
        memcpy(&key, sec0 + 0x0AF8, 4);
    else
        memcpy(&key, sec0 + 0x00AC, 4);
    return key;
}

static uint16_t g3_money_off(pksave_game_t g)
    { return (g == PKSAVE_GAME_FRLG) ? 0x290U : 0x490U; }
static uint16_t g3_team_cnt_off(pksave_game_t g)
    { return (g == PKSAVE_GAME_FRLG) ? 0x034U : 0x234U; }
static uint16_t g3_team_data_off(pksave_game_t g)
    { return (g == PKSAVE_GAME_FRLG) ? 0x038U : 0x238U; }

/* ── Pokemon structure ───────────────────────────────────────────────────── */

#define G3_MON_SIZE     100U
#define G3_BOX_MON_SIZE  80U

static const uint8_t g3_suborder[24][4] = {
    {0,1,2,3},{0,1,3,2},{0,2,1,3},{0,2,3,1},{0,3,1,2},{0,3,2,1},
    {1,0,2,3},{1,0,3,2},{1,2,0,3},{1,2,3,0},{1,3,0,2},{1,3,2,0},
    {2,0,1,3},{2,0,3,1},{2,1,0,3},{2,1,3,0},{2,3,0,1},{2,3,1,0},
    {3,0,1,2},{3,0,2,1},{3,1,0,2},{3,1,2,0},{3,2,0,1},{3,2,1,0},
};

/* XOR decrypt/encrypt 48 bytes with PRNG seeded by key (operation is its own inverse). */
static void g3_crypt(const uint8_t *in, uint32_t key, uint8_t *out)
{
    uint32_t seed = key;
    for (int i = 0; i < 48; i += 4) {
        seed = 0x41C64E6Du * seed + 0x6073u;
        uint32_t w;
        memcpy(&w, in + i, 4);
        w ^= seed;
        memcpy(out + i, &w, 4);
    }
}

static void g3_decode_mon(const uint8_t *mon, pksave_pokemon_t *out)
{
    memset(out, 0, sizeof(*out));

    uint32_t pid; memcpy(&pid, mon, 4);
    uint16_t tid; memcpy(&tid, mon + 4, 2);
    uint16_t sid; memcpy(&sid, mon + 6, 2);

    uint32_t key = pid ^ ((uint32_t)tid | ((uint32_t)sid << 16));

    uint8_t dec[48];
    g3_crypt(mon + 0x20, key, dec);

    const uint8_t *ord = g3_suborder[pid % 24];
    int gp = 0, ap = 0, ep = 0, mp = 0;
    for (int i = 0; i < 4; i++) {
        if      (ord[i] == 0) gp = i * 12;
        else if (ord[i] == 1) ap = i * 12;
        else if (ord[i] == 2) ep = i * 12;
        else                  mp = i * 12;
    }

    /* Growth: species(2), item(2), exp(4), pp_bonuses(1), friendship(1) */
    uint16_t sp; memcpy(&sp, dec + gp, 2);     out->species   = sp;
    uint16_t it; memcpy(&it, dec + gp + 2, 2); out->held_item = it;

    /* Attacks: move1-4(2 each) at ap+0, pp1-4(1 each) at ap+8 */
    for (int i = 0; i < 4; i++) {
        uint16_t mv; memcpy(&mv, dec + ap + i*2, 2);
        out->move_ids[i] = mv;
        out->move_pp[i]  = dec[ap + 8 + i];
    }

    /* EVs at ep: HP,Atk,Def,Spd,SpA,SpD (1 each) */
    out->evs[0] = dec[ep];     out->evs[1] = dec[ep+1];
    out->evs[2] = dec[ep+2];   out->evs[5] = dec[ep+3];
    out->evs[3] = dec[ep+4];   out->evs[4] = dec[ep+5];

    /* Misc IVs: bits 0-4=HP, 5-9=Atk, 10-14=Def, 15-19=Spd, 20-24=SpA, 25-29=SpD */
    uint32_t iv; memcpy(&iv, dec + mp, 4);
    out->ivs[0] = (uint8_t)((iv)      & 0x1F);
    out->ivs[1] = (uint8_t)((iv >> 5) & 0x1F);
    out->ivs[2] = (uint8_t)((iv >>10) & 0x1F);
    out->ivs[5] = (uint8_t)((iv >>15) & 0x1F);
    out->ivs[3] = (uint8_t)((iv >>20) & 0x1F);
    out->ivs[4] = (uint8_t)((iv >>25) & 0x1F);

    gen3_decode_name(mon + 0x14, out->ot_name, sizeof(out->ot_name), 7);
    gen3_decode_name(mon + 0x08, out->nickname, sizeof(out->nickname), 10);

    out->level = mon[0x52];

    uint16_t v;
    memcpy(&v, mon + 0x56, 2); out->stat[0] = v;
    memcpy(&v, mon + 0x58, 2); out->stat[1] = v;
    memcpy(&v, mon + 0x5A, 2); out->stat[2] = v;
    memcpy(&v, mon + 0x5C, 2); out->stat[5] = v;
    memcpy(&v, mon + 0x5E, 2); out->stat[3] = v;
    memcpy(&v, mon + 0x60, 2); out->stat[4] = v;
}

/* ── public genN API ─────────────────────────────────────────────────────── */

int gen3_get_trainer(pksave_save_t *s, pksave_trainer_t *out)
{
    uint32_t slot_off = g3_active_slot(s->sram);
    size_t s0 = g3_find_section(s->sram, slot_off, 0);
    size_t s1 = g3_find_section(s->sram, slot_off, 1);
    if (s0 == SIZE_MAX || s1 == SIZE_MAX) return PKSAVE_ERR_CORRUPT;

    const uint8_t *sec0 = s->sram + s0;
    const uint8_t *sec1 = s->sram + s1;

    memset(out, 0, sizeof(*out));
    gen3_decode_name(sec0, out->name, sizeof(out->name), 7);
    memcpy(&out->tid, sec0 + 0x00A, 2);
    memcpy(&out->sid, sec0 + 0x00C, 2);
    out->badges = sec0[0x083];

    uint32_t key = g3_security_key(sec0, s->game);
    uint32_t money_enc;
    memcpy(&money_enc, sec1 + g3_money_off(s->game), 4);
    out->money = money_enc ^ key;
    return PKSAVE_OK;
}

int gen3_set_trainer(pksave_save_t *s, const pksave_trainer_t *in)
{
    uint32_t slot_off = g3_active_slot(s->sram);
    size_t s0 = g3_find_section(s->sram, slot_off, 0);
    size_t s1 = g3_find_section(s->sram, slot_off, 1);
    if (s0 == SIZE_MAX || s1 == SIZE_MAX) return PKSAVE_ERR_CORRUPT;

    uint8_t *sec0 = s->sram + s0;
    uint8_t *sec1 = s->sram + s1;

    gen3_encode_name(in->name, sec0, 7);
    memcpy(sec0 + 0x00A, &in->tid, 2);
    memcpy(sec0 + 0x00C, &in->sid, 2);
    sec0[0x083] = in->badges;

    uint32_t key = g3_security_key(sec0, s->game);
    uint32_t money_enc = in->money ^ key;
    memcpy(sec1 + g3_money_off(s->game), &money_enc, 4);
    return PKSAVE_OK;
}

int gen3_get_party(pksave_save_t *s, pksave_pokemon_t out[6], uint8_t *count)
{
    uint32_t slot_off = g3_active_slot(s->sram);
    size_t s1 = g3_find_section(s->sram, slot_off, 1);
    if (s1 == SIZE_MAX) return PKSAVE_ERR_CORRUPT;
    const uint8_t *sec1 = s->sram + s1;

    uint32_t cnt;
    memcpy(&cnt, sec1 + g3_team_cnt_off(s->game), 4);
    if (cnt > 6) cnt = 6;
    *count = (uint8_t)cnt;

    uint16_t doff = g3_team_data_off(s->game);
    for (uint32_t i = 0; i < cnt; i++)
        g3_decode_mon(sec1 + doff + i * G3_MON_SIZE, &out[i]);
    return PKSAVE_OK;
}

/* Encode mon into the 100-byte party slot starting at `dst`.
 * Decrypts existing substructs first so untouched fields (exp, friendship,
 * contest stats, met data, etc.) survive the write.  Then overwrites the
 * fields exposed by pksave_pokemon_t, recomputes the checksum, and
 * re-encrypts. */
static void g3_encode_mon(uint8_t *dst, const pksave_pokemon_t *mon,
                          uint16_t trainer_tid, uint16_t trainer_sid)
{
    uint32_t pid; memcpy(&pid, dst, 4);
    uint16_t tid; memcpy(&tid, dst + 4, 2);
    uint16_t sid; memcpy(&sid, dst + 6, 2);
    if (tid == 0 && sid == 0) {  /* fresh slot — inherit from trainer */
        tid = trainer_tid;
        sid = trainer_sid;
        memcpy(dst + 4, &tid, 2);
        memcpy(dst + 6, &sid, 2);
    }

    uint32_t key = pid ^ ((uint32_t)tid | ((uint32_t)sid << 16));

    uint8_t dec[48];
    g3_crypt(dst + 0x20, key, dec);

    const uint8_t *ord = g3_suborder[pid % 24];
    int gp = 0, ap = 0, ep = 0, mp = 0;
    for (int i = 0; i < 4; i++) {
        if      (ord[i] == 0) gp = i * 12;
        else if (ord[i] == 1) ap = i * 12;
        else if (ord[i] == 2) ep = i * 12;
        else                  mp = i * 12;
    }

    /* Growth: species(2), held_item(2) */
    uint16_t sp = mon->species;     memcpy(dec + gp,     &sp, 2);
    uint16_t it = mon->held_item;   memcpy(dec + gp + 2, &it, 2);

    /* Attacks: 4 moves (2 each), 4 PP (1 each) */
    for (int i = 0; i < 4; i++) {
        uint16_t mv = mon->move_ids[i];
        memcpy(dec + ap + i*2, &mv, 2);
        dec[ap + 8 + i] = mon->move_pp[i];
    }

    /* EVs at ep+0..5: HP,Atk,Def,Spd,SpA,SpD (1 each, Gen III capped at 255) */
    dec[ep + 0] = (uint8_t)mon->evs[0];
    dec[ep + 1] = (uint8_t)mon->evs[1];
    dec[ep + 2] = (uint8_t)mon->evs[2];
    dec[ep + 3] = (uint8_t)mon->evs[5];
    dec[ep + 4] = (uint8_t)mon->evs[3];
    dec[ep + 5] = (uint8_t)mon->evs[4];

    /* Misc IVs: bits 0-4=HP, 5-9=Atk, 10-14=Def, 15-19=Spd, 20-24=SpA, 25-29=SpD;
     * preserve bits 30-31 (egg/nicknamed flags). */
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

    /* Checksum = 16-bit sum of uint16 words in unencrypted substructs */
    uint32_t sum = 0;
    for (int i = 0; i < 48; i += 2) {
        uint16_t w; memcpy(&w, dec + i, 2);
        sum += w;
    }
    uint16_t checksum = (uint16_t)sum;
    memcpy(dst + 0x1C, &checksum, 2);

    /* Encrypt back into the slot */
    g3_crypt(dec, key, dst + 0x20);

    /* OT name and nickname (raw byte writes; charset handled by gen3_encode_name) */
    gen3_encode_name(mon->ot_name,  dst + 0x14, 7);
    gen3_encode_name(mon->nickname, dst + 0x08, 10);

    /* Party extras: status@0x50(4), level@0x52, mail@0x53, currentHP@0x54,
     * maxHP@0x56, Atk@0x58, Def@0x5A, Spd@0x5C, SpA@0x5E, SpD@0x60 */
    dst[0x52] = mon->level;

    uint16_t v;
    v = mon->stat[0]; memcpy(dst + 0x54, &v, 2);  /* current HP = max */
    v = mon->stat[0]; memcpy(dst + 0x56, &v, 2);
    v = mon->stat[1]; memcpy(dst + 0x58, &v, 2);
    v = mon->stat[2]; memcpy(dst + 0x5A, &v, 2);
    v = mon->stat[5]; memcpy(dst + 0x5C, &v, 2);
    v = mon->stat[3]; memcpy(dst + 0x5E, &v, 2);
    v = mon->stat[4]; memcpy(dst + 0x60, &v, 2);
}

int gen3_set_party_slot(pksave_save_t *s, uint8_t slot, const pksave_pokemon_t *mon)
{
    if (slot >= 6) return PKSAVE_ERR_RANGE;

    uint32_t slot_off = g3_active_slot(s->sram);
    size_t s0 = g3_find_section(s->sram, slot_off, 0);
    size_t s1 = g3_find_section(s->sram, slot_off, 1);
    if (s0 == SIZE_MAX || s1 == SIZE_MAX) return PKSAVE_ERR_CORRUPT;

    uint8_t *sec0 = s->sram + s0;
    uint8_t *sec1 = s->sram + s1;

    uint16_t trainer_tid, trainer_sid;
    memcpy(&trainer_tid, sec0 + 0x00A, 2);
    memcpy(&trainer_sid, sec0 + 0x00C, 2);

    uint32_t cnt;
    memcpy(&cnt, sec1 + g3_team_cnt_off(s->game), 4);

    uint16_t doff = g3_team_data_off(s->game);
    uint8_t *dst = sec1 + doff + (size_t)slot * G3_MON_SIZE;

    g3_encode_mon(dst, mon, trainer_tid, trainer_sid);

    if ((uint32_t)slot >= cnt) {
        cnt = (uint32_t)slot + 1;
        memcpy(sec1 + g3_team_cnt_off(s->game), &cnt, 4);
    }
    return PKSAVE_OK;
}

int gen3_get_box(pksave_save_t *s, uint8_t box, pksave_pokemon_t out[30], uint8_t *cnt)
{
    (void)s; (void)box; (void)out;
    *cnt = 0;
    return PKSAVE_OK;
}

int gen3_set_box_slot(pksave_save_t *s, uint8_t box, uint8_t slot,
                       const pksave_pokemon_t *mon)
{
    (void)s; (void)box; (void)slot; (void)mon;
    return PKSAVE_ERR_RANGE;
}

int gen3_get_pocket(pksave_save_t *s, pksave_pocket_t p, pksave_item_t *out, uint8_t *cnt)
{
    (void)s; (void)p; (void)out;
    *cnt = 0;
    return PKSAVE_OK;
}

int gen3_set_item(pksave_save_t *s, pksave_pocket_t p, uint8_t slot,
                   const pksave_item_t *item)
{
    (void)s; (void)p; (void)slot; (void)item;
    return PKSAVE_ERR_RANGE;
}

/* ── checksum ────────────────────────────────────────────────────────────── */

int gen3_flush(pksave_save_t *s)
{
    uint32_t slot_off = g3_active_slot(s->sram);
    for (uint32_t i = 0; i < G3_NUM_SECTIONS; i++) {
        size_t off = slot_off + (size_t)i * G3_SECTION_SIZE;
        uint32_t sig;
        memcpy(&sig, s->sram + off + G3_SIG_OFF, 4);
        if (sig == G3_SIG)
            g3_update_chksum(s->sram + off);
    }
    return PKSAVE_OK;
}

int gen3_validate_checksum(const uint8_t *sram, size_t len, pksave_game_t game)
{
    (void)game;
    if (len < 128U * 1024U) return PKSAVE_ERR_CORRUPT;

    uint32_t slot_off = g3_active_slot(sram);

    for (uint32_t s = 0; s < G3_NUM_SECTIONS; s++) {
        size_t off = slot_off + (size_t)s * G3_SECTION_SIZE;
        uint32_t sig;
        memcpy(&sig, sram + off + G3_SIG_OFF, 4);
        if (sig != G3_SIG) return PKSAVE_ERR_CORRUPT;

        uint16_t id, stored, calc;
        memcpy(&id,     sram + off + G3_ID_OFF,    2);
        memcpy(&stored, sram + off + G3_CHKSUM_OFF, 2);
        calc = g3_section_chksum(sram + off, id);
        if (stored != calc) return PKSAVE_ERR_CORRUPT;
    }
    return PKSAVE_OK;
}

#endif /* PKSAVE_GEN3 */
