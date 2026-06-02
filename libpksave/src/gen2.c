/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Gen II (Gold/Silver, Crystal) save format implementation.
 * Offsets from pret/pokecrystal engine/save.asm
 *
 * .sav file layout (32KB):
 *   Bank 0 (0x0000-0x1FFF): Hall of Fame data
 *   Bank 1 (0x2000-0x3FFF): player save data
 *   Banks 4-7 (depends on game): PC boxes
 *
 * Key offsets (bank 1, same for GS and Crystal):
 *   0x2008: trainer ID       (2 bytes, big-endian)
 *   0x200B: player name      (11 bytes, Gen I/II charset)
 *
 * GS-specific:
 *   0x23DB: money            (3 bytes BCD, big-endian)
 *   0x23E4: Johto badges     (1 byte bitfield)
 *   0x288A: party count      (1 byte)
 *   0x288B: party species    (7 bytes: 6 + 0xFF)
 *   0x2892: party data       (6 × 48 bytes)
 *   Checksum: sum(0x2009..0x2D68) stored LE at 0x2D69
 *
 * Crystal-specific:
 *   0x23DC: money            (3 bytes BCD, big-endian)
 *   0x23E5: Johto badges     (1 byte bitfield)
 *   0x2865: party count      (1 byte)
 *   0x2866: party species    (7 bytes: 6 + 0xFF)
 *   0x286D: party data       (6 × 48 bytes)
 *   Primary checksum:   sum(0x2009..0x2B82) LE at 0x2D0D
 *   Secondary checksum: sum(0x1209..0x1D82) LE at 0x1F0D
 *
 * Party mon struct (48 bytes):
 *   0x00: species            0x01: held item
 *   0x02-0x05: moves         0x06-0x07: OT ID (BE)
 *   0x08-0x0A: exp (3, BE)   0x0B-0x14: stat exp [HP,Atk,Def,Spd,Spc] (5×2 BE)
 *   0x15-0x16: DVs           0x17-0x1A: move PP
 *   0x1B: friendship         0x1C: pokerus strain
 *   0x1D: pokerus duration   0x1E-0x1F: caught_data
 *   0x20: level              0x21: status
 *   0x22-0x23: current HP (BE)
 *   0x24-0x2F: stats BE [maxHP, Atk, Def, Spd, SpA, SpD]
 *
 * Box mon struct (32 bytes): same as party mon 0x00-0x1F.
 */
#include "pksave_internal.h"

#ifdef PKSAVE_GEN2

/* ── shared offsets ──────────────────────────────────────────────────────── */
#define G2_TID          0x2008U
#define G2_PLAYER_NAME  0x200BU

/* GS offsets */
#define GS_MONEY        0x23DBU
#define GS_BADGES       0x23E4U
#define GS_PARTY_CNT    0x288AU
#define GS_PARTY_SPEC   0x288BU
#define GS_PARTY_DATA   0x2892U
#define GS_PARTY_OT     (GS_PARTY_DATA + 6 * 48U)
#define GS_PARTY_NICK   (GS_PARTY_OT   + 6 * 11U)

#define GS_CHKSUM_START 0x2009U
#define GS_CHKSUM_END   0x2D68U
#define GS_CHKSUM_ADDR  0x2D69U

/* Crystal offsets */
#define CRY_MONEY       0x23DCU
#define CRY_BADGES      0x23E5U
#define CRY_PARTY_CNT   0x2865U
#define CRY_PARTY_SPEC  0x2866U
#define CRY_PARTY_DATA  0x286DU
#define CRY_PARTY_OT    (CRY_PARTY_DATA + 6 * 48U)
#define CRY_PARTY_NICK  (CRY_PARTY_OT   + 6 * 11U)

#define CRY_CHKSUM1_START 0x2009U
#define CRY_CHKSUM1_END   0x2B82U
#define CRY_CHKSUM1_ADDR  0x2D0DU
#define CRY_CHKSUM2_START 0x1209U
#define CRY_CHKSUM2_END   0x1D82U
#define CRY_CHKSUM2_ADDR  0x1F0DU

#define G2_PARTY_MON_SIZE  48U
#define G2_BOX_MON_SIZE    32U
#define G2_NAME_LEN        11U
#define G2_BOX_PER_BOX     20U
#define G2_NUM_BOXES_GS    14U
#define G2_NUM_BOXES_CRY   14U

/* ── character encoding (shared with Gen I) ──────────────────────────────── */

static char gen2_decode_char(uint8_t b)
{
    if (b == 0x50 || b == 0x00) return '\0';
    if (b == 0x7F)              return ' ';
    if (b >= 0x80 && b <= 0x99) return (char)('A' + (b - 0x80));
    if (b >= 0xA0 && b <= 0xB9) return (char)('a' + (b - 0xA0));
    if (b >= 0xF6)              return (char)('0' + (b - 0xF6));
    if (b == 0xE0)              return '\'';
    if (b == 0xE3)              return '-';
    return '?';
}

static uint8_t gen2_encode_char(char c)
{
    if (c == '\0') return 0x50;
    if (c == ' ')  return 0x7F;
    if (c >= 'A' && c <= 'Z') return (uint8_t)(0x80 + (c - 'A'));
    if (c >= 'a' && c <= 'z') return (uint8_t)(0xA0 + (c - 'a'));
    if (c >= '0' && c <= '9') return (uint8_t)(0xF6 + (c - '0'));
    if (c == '\'') return 0xE0;
    if (c == '-')  return 0xE3;
    return 0x50;
}

static void gen2_decode_name(const uint8_t *src, char *dst, int max_len)
{
    for (int i = 0; i < (int)G2_NAME_LEN && i < max_len - 1; i++) {
        if (src[i] == 0x50) break;
        char c = gen2_decode_char(src[i]);
        if (c == '\0') break;
        *dst++ = c;
    }
    *dst = '\0';
}

static void gen2_encode_name(const char *src, uint8_t *dst)
{
    int i;
    for (i = 0; i < (int)G2_NAME_LEN && src[i] != '\0'; i++)
        dst[i] = gen2_encode_char(src[i]);
    for (; i < (int)G2_NAME_LEN; i++)
        dst[i] = 0x50;
}

/* ── BCD helpers ─────────────────────────────────────────────────────────── */

static uint32_t bcd3_decode(const uint8_t *p)
{
    return ((uint32_t)(p[0] >> 4) * 100000)
         + ((uint32_t)(p[0] & 0x0F) * 10000)
         + ((uint32_t)(p[1] >> 4) * 1000)
         + ((uint32_t)(p[1] & 0x0F) * 100)
         + ((uint32_t)(p[2] >> 4) * 10)
         + ((uint32_t)(p[2] & 0x0F));
}

static void bcd3_encode(uint32_t v, uint8_t *p)
{
    if (v > 999999) v = 999999;
    p[0] = (uint8_t)(((v / 100000) << 4) | ((v / 10000) % 10));
    p[1] = (uint8_t)((((v / 1000) % 10) << 4) | ((v / 100) % 10));
    p[2] = (uint8_t)((((v / 10) % 10) << 4) | (v % 10));
}

/* ── checksum helpers ────────────────────────────────────────────────────── */

static uint16_t gen2_sum16(const uint8_t *sram, uint32_t start, uint32_t end)
{
    uint32_t s = 0;
    for (uint32_t i = start; i <= end; i++) s += sram[i];
    return (uint16_t)(s & 0xFFFF);
}

static uint16_t gen2_read_le16(const uint8_t *sram, uint32_t off)
{
    return (uint16_t)(sram[off] | ((uint16_t)sram[off + 1] << 8));
}

static void gen2_write_le16(uint8_t *sram, uint32_t off, uint16_t v)
{
    sram[off]     = (uint8_t)(v & 0xFF);
    sram[off + 1] = (uint8_t)(v >> 8);
}

/* ── party mon decode / encode ───────────────────────────────────────────── */

static void gen2_decode_party_mon(const uint8_t *data,
                                   const uint8_t *ot_name,
                                   const uint8_t *nick,
                                   pksave_pokemon_t *out)
{
    memset(out, 0, sizeof(*out));
    out->species   = data[0x00];
    out->held_item = data[0x01];
    out->level     = data[0x20];
    out->nature    = 0;
    out->ability   = 0;

    for (int i = 0; i < 4; i++) {
        out->move_ids[i] = data[0x02 + i];
        out->move_pp[i]  = data[0x17 + i];
    }

    /* stat exp → evs: [HP,Atk,Def,Spd,Spc] stored as 5×2 BE */
    for (int i = 0; i < 5; i++) {
        uint16_t se = (uint16_t)((data[0x0B + i*2] << 8) | data[0x0C + i*2]);
        switch (i) {
            case 0: out->evs[0] = se; break; /* HP */
            case 1: out->evs[1] = se; break; /* Atk */
            case 2: out->evs[2] = se; break; /* Def */
            case 3: out->evs[5] = se; break; /* Spd → Spe */
            case 4: out->evs[3] = out->evs[4] = se; break; /* Spc → SpA & SpD */
        }
    }

    /* DVs: same layout as Gen I [Atk4|Def4|Spd4|Spc4], scaled to 0-31 */
    uint16_t dvs  = (uint16_t)((data[0x15] << 8) | data[0x16]);
    uint8_t dv_atk = (dvs >> 12) & 0x0F;
    uint8_t dv_def = (dvs >>  8) & 0x0F;
    uint8_t dv_spd = (dvs >>  4) & 0x0F;
    uint8_t dv_spc = (dvs >>  0) & 0x0F;
    uint8_t dv_hp  = (uint8_t)(((dv_atk & 1) << 3) | ((dv_def & 1) << 2)
                              | ((dv_spd & 1) << 1) | (dv_spc & 1));
    out->ivs[0] = (uint8_t)(dv_hp  * 2);
    out->ivs[1] = (uint8_t)(dv_atk * 2);
    out->ivs[2] = (uint8_t)(dv_def * 2);
    out->ivs[5] = (uint8_t)(dv_spd * 2);
    out->ivs[3] = out->ivs[4] = (uint8_t)(dv_spc * 2);

    /* Gen II shininess: Spd==10, Def==10, Spc==10, Atk even&>=2 */
    out->is_shiny = (dv_spd == 10 && dv_def == 10 && dv_spc == 10
                     && (dv_atk == 2 || dv_atk == 3 || dv_atk == 6 || dv_atk == 7
                         || dv_atk == 10 || dv_atk == 11 || dv_atk == 14
                         || dv_atk == 15)) ? 1 : 0;

    /* Stats: maxHP, Atk, Def, Spd, SpA, SpD (6×2 BE at 0x24) */
    out->stat[0] = (uint16_t)((data[0x24] << 8) | data[0x25]);
    out->stat[1] = (uint16_t)((data[0x26] << 8) | data[0x27]);
    out->stat[2] = (uint16_t)((data[0x28] << 8) | data[0x29]);
    out->stat[5] = (uint16_t)((data[0x2A] << 8) | data[0x2B]);
    out->stat[3] = (uint16_t)((data[0x2C] << 8) | data[0x2D]);
    out->stat[4] = (uint16_t)((data[0x2E] << 8) | data[0x2F]);

    if (ot_name) gen2_decode_name(ot_name, out->ot_name, sizeof(out->ot_name));
    if (nick)    gen2_decode_name(nick,    out->nickname, sizeof(out->nickname));
}

static void gen2_encode_party_mon(const pksave_pokemon_t *in,
                                   uint8_t *data,
                                   uint8_t *ot_name,
                                   uint8_t *nick)
{
    memset(data, 0, G2_PARTY_MON_SIZE);
    data[0x00] = (uint8_t)in->species;
    data[0x01] = (uint8_t)in->held_item;
    data[0x20] = in->level;

    for (int i = 0; i < 4; i++) {
        data[0x02 + i] = in->move_ids[i];
        data[0x17 + i] = in->move_pp[i];
    }

    /* stat exp */
    uint16_t se[5];
    se[0] = in->evs[0];
    se[1] = in->evs[1];
    se[2] = in->evs[2];
    se[3] = in->evs[5]; /* Spe → Spd */
    se[4] = in->evs[3]; /* SpA → Spc */
    for (int i = 0; i < 5; i++) {
        data[0x0B + i*2] = (uint8_t)(se[i] >> 8);
        data[0x0C + i*2] = (uint8_t)(se[i] & 0xFF);
    }

    /* DVs */
    uint8_t dv_atk = (uint8_t)(in->ivs[1] / 2);
    uint8_t dv_def = (uint8_t)(in->ivs[2] / 2);
    uint8_t dv_spd = (uint8_t)(in->ivs[5] / 2);
    uint8_t dv_spc = (uint8_t)(in->ivs[3] / 2);
    uint16_t dvs   = (uint16_t)((dv_atk << 12) | (dv_def << 8)
                               | (dv_spd << 4)  | dv_spc);
    data[0x15] = (uint8_t)(dvs >> 8);
    data[0x16] = (uint8_t)(dvs & 0xFF);

    /* stats */
    data[0x24] = (uint8_t)(in->stat[0] >> 8); data[0x25] = (uint8_t)in->stat[0];
    data[0x26] = (uint8_t)(in->stat[1] >> 8); data[0x27] = (uint8_t)in->stat[1];
    data[0x28] = (uint8_t)(in->stat[2] >> 8); data[0x29] = (uint8_t)in->stat[2];
    data[0x2A] = (uint8_t)(in->stat[5] >> 8); data[0x2B] = (uint8_t)in->stat[5];
    data[0x2C] = (uint8_t)(in->stat[3] >> 8); data[0x2D] = (uint8_t)in->stat[3];
    data[0x2E] = (uint8_t)(in->stat[4] >> 8); data[0x2F] = (uint8_t)in->stat[4];

    /* current HP = max HP */
    data[0x22] = data[0x24]; data[0x23] = data[0x25];

    if (ot_name) gen2_encode_name(in->ot_name,  ot_name);
    if (nick)    gen2_encode_name(in->nickname, nick);
}

/* ── game-variant accessors ──────────────────────────────────────────────── */

static int is_crystal(pksave_game_t g) { return g == PKSAVE_GAME_CRYSTAL; }

static uint32_t money_off(pksave_game_t g)
    { return is_crystal(g) ? CRY_MONEY   : GS_MONEY; }
static uint32_t badges_off(pksave_game_t g)
    { return is_crystal(g) ? CRY_BADGES  : GS_BADGES; }
static uint32_t party_cnt_off(pksave_game_t g)
    { return is_crystal(g) ? CRY_PARTY_CNT  : GS_PARTY_CNT; }
static uint32_t party_spec_off(pksave_game_t g)
    { return is_crystal(g) ? CRY_PARTY_SPEC : GS_PARTY_SPEC; }
static uint32_t party_data_off(pksave_game_t g)
    { return is_crystal(g) ? CRY_PARTY_DATA : GS_PARTY_DATA; }
static uint32_t party_ot_off(pksave_game_t g)
    { return is_crystal(g) ? CRY_PARTY_OT : GS_PARTY_OT; }
static uint32_t party_nick_off(pksave_game_t g)
    { return is_crystal(g) ? CRY_PARTY_NICK : GS_PARTY_NICK; }

/* ── public genN API ─────────────────────────────────────────────────────── */

int gen2_get_trainer(pksave_save_t *s, pksave_trainer_t *out)
{
    memset(out, 0, sizeof(*out));
    gen2_decode_name(s->sram + G2_PLAYER_NAME, out->name, sizeof(out->name));
    out->tid    = (uint16_t)((s->sram[G2_TID] << 8) | s->sram[G2_TID + 1]);
    out->sid    = 0;
    out->money  = bcd3_decode(s->sram + money_off(s->game));
    out->badges = s->sram[badges_off(s->game)];
    return PKSAVE_OK;
}

int gen2_set_trainer(pksave_save_t *s, const pksave_trainer_t *in)
{
    gen2_encode_name(in->name, s->sram + G2_PLAYER_NAME);
    s->sram[G2_TID]     = (uint8_t)(in->tid >> 8);
    s->sram[G2_TID + 1] = (uint8_t)(in->tid & 0xFF);
    bcd3_encode(in->money, s->sram + money_off(s->game));
    s->sram[badges_off(s->game)] = in->badges;
    return PKSAVE_OK;
}

int gen2_get_party(pksave_save_t *s, pksave_pokemon_t out[6], uint8_t *count)
{
    uint8_t cnt = s->sram[party_cnt_off(s->game)];
    if (cnt > 6) cnt = 6;
    *count = cnt;
    uint32_t data_off = party_data_off(s->game);
    uint32_t ot_off   = party_ot_off(s->game);
    uint32_t nick_off = party_nick_off(s->game);
    for (int i = 0; i < cnt; i++) {
        const uint8_t *data = s->sram + data_off + (size_t)i * G2_PARTY_MON_SIZE;
        const uint8_t *ot   = s->sram + ot_off   + (size_t)i * G2_NAME_LEN;
        const uint8_t *nick = s->sram + nick_off  + (size_t)i * G2_NAME_LEN;
        gen2_decode_party_mon(data, ot, nick, &out[i]);
    }
    return PKSAVE_OK;
}

int gen2_set_party_slot(pksave_save_t *s, uint8_t slot, const pksave_pokemon_t *mon)
{
    if (slot >= 6) return PKSAVE_ERR_RANGE;
    uint32_t data_off = party_data_off(s->game);
    uint32_t ot_off   = party_ot_off(s->game);
    uint32_t nick_off = party_nick_off(s->game);
    uint8_t *data = s->sram + data_off + (size_t)slot * G2_PARTY_MON_SIZE;
    uint8_t *ot   = s->sram + ot_off   + (size_t)slot * G2_NAME_LEN;
    uint8_t *nick = s->sram + nick_off  + (size_t)slot * G2_NAME_LEN;
    gen2_encode_party_mon(mon, data, ot, nick);
    s->sram[party_spec_off(s->game) + slot] = (uint8_t)mon->species;
    uint8_t cnt = s->sram[party_cnt_off(s->game)];
    if (slot >= cnt) {
        s->sram[party_cnt_off(s->game)] = slot + 1;
        s->sram[party_spec_off(s->game) + slot + 1] = 0xFF;
    }
    return PKSAVE_OK;
}

int gen2_get_box(pksave_save_t *s, uint8_t box, pksave_pokemon_t out[30], uint8_t *cnt)
{
    (void)s; (void)box; (void)out;
    *cnt = 0;
    return PKSAVE_OK;
}

int gen2_set_box_slot(pksave_save_t *s, uint8_t box, uint8_t slot,
                       const pksave_pokemon_t *mon)
{
    (void)s; (void)box; (void)slot; (void)mon;
    return PKSAVE_ERR_RANGE;
}

int gen2_get_pocket(pksave_save_t *s, pksave_pocket_t p, pksave_item_t *out, uint8_t *cnt)
{
    (void)s; (void)p; (void)out;
    *cnt = 0;
    return PKSAVE_OK;
}

int gen2_set_item(pksave_save_t *s, pksave_pocket_t p, uint8_t slot,
                   const pksave_item_t *item)
{
    (void)s; (void)p; (void)slot; (void)item;
    return PKSAVE_ERR_RANGE;
}

/* ── checksum ────────────────────────────────────────────────────────────── */

int gen2_validate_checksum(const uint8_t *sram, size_t len, pksave_game_t game)
{
    (void)len;
    if (game == PKSAVE_GAME_CRYSTAL) {
        if (gen2_sum16(sram, CRY_CHKSUM1_START, CRY_CHKSUM1_END) !=
            gen2_read_le16(sram, CRY_CHKSUM1_ADDR)) return PKSAVE_ERR_CORRUPT;
        if (gen2_sum16(sram, CRY_CHKSUM2_START, CRY_CHKSUM2_END) !=
            gen2_read_le16(sram, CRY_CHKSUM2_ADDR)) return PKSAVE_ERR_CORRUPT;
        return PKSAVE_OK;
    }
    /* GS */
    return gen2_sum16(sram, GS_CHKSUM_START, GS_CHKSUM_END) ==
           gen2_read_le16(sram, GS_CHKSUM_ADDR) ? PKSAVE_OK : PKSAVE_ERR_CORRUPT;
}

int gen2_flush(pksave_save_t *s)
{
    if (s->game == PKSAVE_GAME_CRYSTAL) {
        gen2_write_le16(s->sram, CRY_CHKSUM1_ADDR,
                        gen2_sum16(s->sram, CRY_CHKSUM1_START, CRY_CHKSUM1_END));
        gen2_write_le16(s->sram, CRY_CHKSUM2_ADDR,
                        gen2_sum16(s->sram, CRY_CHKSUM2_START, CRY_CHKSUM2_END));
    } else {
        gen2_write_le16(s->sram, GS_CHKSUM_ADDR,
                        gen2_sum16(s->sram, GS_CHKSUM_START, GS_CHKSUM_END));
    }
    return PKSAVE_OK;
}

#endif /* PKSAVE_GEN2 */
