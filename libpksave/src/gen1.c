/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 *
 * Gen I (Red/Blue/Yellow) save format implementation.
 * All offsets derived from pret/pokered ram/sram.asm and engine/save_menu.asm.
 *
 * .sav file layout (32KB):
 *   Bank 0 (0x0000-0x1FFF): sprite buffers, Hall of Fame
 *   Bank 1 (0x2000-0x3FFF): player save data
 *   Banks 2-3 (0x4000-0x7FFF): PC boxes 1-12
 *
 * Key offsets in bank 1:
 *   0x2598: player name       (11 bytes, Gen I charset, 0x50-terminated)
 *   0x25F3: money             (3 bytes BCD, big-endian)
 *   0x2602: badges            (1 byte bitfield)
 *   0x2605: trainer ID        (2 bytes big-endian)
 *   0x284C: current box number (1 byte, 0-11)
 *   0x2F2C: party count       (1 byte, 0-6)
 *   0x2F2D: party species list (7 bytes: 6 species + 0xFF terminator)
 *   0x2F34: party mon data    (6 × 44 bytes)
 *   0x303C: party OT names    (6 × 11 bytes)
 *   0x307E: party mon names   (6 × 11 bytes)
 *   0x30C0: current box data  (1122 bytes)
 *   0x3523: main checksum     (1 byte)
 *
 * Party mon struct (44 bytes, pokered PARTYMON_STRUCT_LENGTH = 0x2C):
 *   0x00: species (1)
 *   0x01-0x02: current HP (2, BE)
 *   0x03: box_level / catch_rate (1) — in party context = level copy
 *   0x04: status (1)
 *   0x05-0x06: types (2)
 *   0x07: catch_rate (1)
 *   0x08-0x0B: move IDs (4)
 *   0x0C-0x0D: OT trainer ID (2, BE)
 *   0x0E-0x10: experience (3, BE)
 *   0x11-0x1A: stat experience [HP,Atk,Def,Spd,Spc] (5 × 2 bytes, BE)
 *   0x1B-0x1C: DVs [Atk4|Def4|Spd4|Spc4] (2, BE)
 *   0x1D-0x20: move PP (4)
 *   0x21: level (1)
 *   0x22-0x2B: stats [maxHP,Atk,Def,Spd,Spc] (5 × 2 bytes, BE)
 *
 * Box mon struct (33 bytes): same as party mon without 0x21-0x2B (level + stats).
 *
 * Gen I item: 2 bytes (item_id, quantity). Bag items at 0x25C4.
 * PC items at 0x25C4? Actually bag at 0x25C4, PC box items handled separately.
 */
#include "pksave_internal.h"

#ifdef PKSAVE_GEN1

#include "names/gen1_base_stats.h"

static uint8_t gen1_growth_rate_for_species(uint16_t species_id)
{
    if (species_id < sizeof(gen1_base_stats) / sizeof(gen1_base_stats[0])) {
        return gen1_base_stats[species_id].growth_rate;
    }
    return 0;
}

/* ── offsets ─────────────────────────────────────────────────────────────── */
#define G1_PLAYER_NAME  0x2598U
#define G1_MONEY        0x25F3U
#define G1_BADGES       0x2602U
#define G1_TID          0x2605U
#define G1_CUR_BOX      0x284CU
#define G1_PARTY_CNT    0x2F2CU
#define G1_PARTY_SPEC   0x2F2DU   /* 7 bytes: 6 species + 0xFF */
#define G1_PARTY_DATA   0x2F34U   /* 6 × 44 bytes */
#define G1_PARTY_OT     0x303CU   /* 6 × 11 bytes */
#define G1_PARTY_NAMES  0x307EU   /* 6 × 11 bytes */
#define G1_CUR_BOX_DATA 0x30C0U
#define G1_CHKSUM_START 0x2598U
#define G1_CHKSUM_END   0x3522U
#define G1_CHKSUM_ADDR  0x3523U

/* PC box banks: bank 2 = boxes 1-6 at 0x4000, bank 3 = boxes 7-12 at 0x6000 */
#define G1_BOX_OFFSET(box) ((box) < 6 ? (0x4000U + (box)*1122U) : \
                                        (0x6000U + ((box)-6)*1122U))
#define G1_BOX_CNT_PER_BOX 20
#define G1_BOX_MON_SIZE    33
#define G1_PARTY_MON_SIZE  44
#define G1_NAME_LEN        11

/* Bag items: count byte at 0x25C9, then 20×(id,qty), 0xFF terminator.
 * Layout (42 bytes): 0x25C9 count, 0x25CA..0x25F1 entries, 0x25F2 0xFF.
 * (Per pokered save layout; Money/BCD3 starts at 0x25F3.) */
#define G1_BAG_CNT      0x25C9U
#define G1_BAG_DATA     0x25CAU
#define G1_BAG_MAX      20

/* ── character encoding ──────────────────────────────────────────────────── */
/* Gen I charset: A-Z = 0x80-0x99, a-z = 0xA0-0xB9, space = 0x7F,
 * end = 0x50, digits 0-9 = 0xF6-0xFF */

static char gen1_decode_char(uint8_t b)
{
    if (b == 0x50 || b == 0x00) return '\0';
    if (b == 0x7F)              return ' ';
    if (b >= 0x80 && b <= 0x99) return (char)('A' + (b - 0x80));
    if (b >= 0xA0 && b <= 0xB9) return (char)('a' + (b - 0xA0));
    if (b >= 0xF6) return (char)('0' + (b - 0xF6));
    if (b == 0xE0)              return '\'';
    if (b == 0xE3)              return '-';
    return '?';
}

static uint8_t gen1_encode_char(char c)
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

static void gen1_decode_name(const uint8_t *src, char *dst, int max_len)
{
    for (int i = 0; i < G1_NAME_LEN && i < max_len - 1; i++) {
        if (src[i] == 0x50) break;
        char c = gen1_decode_char(src[i]);
        if (c == '\0') break;
        *dst++ = c;
    }
    *dst = '\0';
}

static void gen1_encode_name(const char *src, uint8_t *dst)
{
    int i;
    for (i = 0; i < G1_NAME_LEN && src[i] != '\0'; i++)
        dst[i] = gen1_encode_char(src[i]);
    for (; i < G1_NAME_LEN; i++)
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

/* ── checksum ────────────────────────────────────────────────────────────── */

#define G1_BOX_SIZE      1122U
#define G1_BANK_BOXES_SZ (6U * G1_BOX_SIZE) /* 0x1A4C */

static uint8_t gen1_sum_bytes(const uint8_t *buf, uint16_t len)
{
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++)
        sum += buf[i];
    return sum;
}

static uint8_t gen1_calc_checksum(const uint8_t *sram)
{
    return (uint8_t)(~gen1_sum_bytes(pksave_sram_cat(sram, G1_CHKSUM_START),
                                     (uint16_t)(G1_CHKSUM_END - G1_CHKSUM_START + 1U)));
}

static void gen1_update_box_bank_checksums(uint8_t *sram, uint32_t bank_base)
{
    /* sBankNAllBoxesChecksum at bank_base + 0x1A4C;
     * sBankNIndividualBoxChecksums[0..5] at bank_base + 0x1A4D..0x1A52. */
    uint8_t *bank = pksave_sram_at(sram, bank_base);
    uint8_t all_sum = 0;
    for (uint8_t b = 0; b < 6U; b++) {
        uint8_t bsum = gen1_sum_bytes(bank + (uint16_t)b * G1_BOX_SIZE, G1_BOX_SIZE);
        bank[G1_BANK_BOXES_SZ + 1U + b] = (uint8_t)(~bsum);
        all_sum = (uint8_t)(all_sum + bsum);
    }
    bank[G1_BANK_BOXES_SZ] = (uint8_t)(~all_sum);
}

static int gen1_validate_box_bank_checksums(const uint8_t *sram, uint32_t bank_base)
{
    const uint8_t *bank = pksave_sram_cat(sram, bank_base);
    uint8_t all_sum = 0;
    for (uint8_t b = 0; b < 6U; b++) {
        uint8_t bsum = gen1_sum_bytes(bank + (uint16_t)b * G1_BOX_SIZE, G1_BOX_SIZE);
        if ((uint8_t)(~bsum) != bank[G1_BANK_BOXES_SZ + 1U + b]) {
            return PKSAVE_ERR_CORRUPT;
        }
        all_sum = (uint8_t)(all_sum + bsum);
    }
    if ((uint8_t)(~all_sum) != bank[G1_BANK_BOXES_SZ]) {
        return PKSAVE_ERR_CORRUPT;
    }
    return PKSAVE_OK;
}

int gen1_validate_checksum(const uint8_t *sram, size_t len, pksave_game_t game)
{
    (void)game;
    if (!sram || len < 0x8000) return PKSAVE_ERR_CORRUPT;
    if (gen1_calc_checksum(sram) != *pksave_sram_cat(sram, G1_CHKSUM_ADDR))
        return PKSAVE_ERR_CORRUPT;
    /* When BIT_HAS_CHANGED_BOXES (bit 7 of wCurrentBoxNum at 0x284C) is set,
     * Banks 2 & 3 carry initialized PC boxes with live checksums. */
    if ((*pksave_sram_cat(sram, G1_CUR_BOX) & 0x80U) != 0) {
        if (gen1_validate_box_bank_checksums(sram, 0x4000U) != PKSAVE_OK)
            return PKSAVE_ERR_CORRUPT;
        if (gen1_validate_box_bank_checksums(sram, 0x6000U) != PKSAVE_OK)
            return PKSAVE_ERR_CORRUPT;
    }
    return PKSAVE_OK;
}

/* ── party mon decode / encode ───────────────────────────────────────────── */

static void gen1_decode_party_mon(const uint8_t *data,
                                   const uint8_t *ot_name,
                                   const uint8_t *nick,
                                   pksave_pokemon_t *out)
{
    memset(out, 0, sizeof(*out));
    out->species   = data[0x00];
    out->level     = data[0x21];
    out->held_item = 0;                  /* Gen I: no held items */
    out->nature    = 0;                  /* Gen I: no natures */
    out->ability   = 0;

    for (int i = 0; i < 4; i++) {
        out->move_ids[i] = data[0x08 + i];
        out->move_pp[i]  = data[0x1D + i];
    }

    /* EVs: 5 stat exp fields × 2 bytes BE → evs[6] (HP,Atk,Def,SpA,SpD,Spe) */
    for (int i = 0; i < 5; i++) {
        uint16_t se = (uint16_t)((data[0x11 + i*2] << 8) | data[0x12 + i*2]);
        /* order in save: HP, Atk, Def, Spd, Spc */
        /* map to evs: [0]=HP [1]=Atk [2]=Def [3]=SpA [4]=SpD [5]=Spe */
        switch (i) {
            case 0: out->evs[0] = se; break; /* HP */
            case 1: out->evs[1] = se; break; /* Atk */
            case 2: out->evs[2] = se; break; /* Def */
            case 3: out->evs[5] = se; break; /* Spd → Spe */
            case 4: out->evs[3] = out->evs[4] = se; break; /* Spc → SpA & SpD */
        }
    }

    /* DVs: 2 bytes [Atk4|Def4|Spd4|Spc4], scaled to 0-31 by doubling */
    uint16_t dvs = (uint16_t)((data[0x1B] << 8) | data[0x1C]);
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

    /* Gen II shininess check on Gen I DVs (not meaningful in-game for Gen I,
     * but expose it for Gen II compatibility display). */
    out->is_shiny = (dv_spd == 10 && dv_def == 10 && dv_spc == 10
                     && (dv_atk == 2 || dv_atk == 3 || dv_atk == 6 || dv_atk == 7
                         || dv_atk == 10 || dv_atk == 11 || dv_atk == 14
                         || dv_atk == 15)) ? 1 : 0;

    /* Stats: maxHP, Atk, Def, Spd, Spc (5 × 2 bytes BE) */
    out->stat[0] = (uint16_t)((data[0x22] << 8) | data[0x23]); /* HP */
    out->stat[1] = (uint16_t)((data[0x24] << 8) | data[0x25]); /* Atk */
    out->stat[2] = (uint16_t)((data[0x26] << 8) | data[0x27]); /* Def */
    out->stat[5] = (uint16_t)((data[0x28] << 8) | data[0x29]); /* Spe */
    out->stat[3] = out->stat[4]
                 = (uint16_t)((data[0x2A] << 8) | data[0x2B]); /* SpA=SpD=Spc */

    gen1_decode_name(ot_name, out->ot_name, sizeof(out->ot_name));
    gen1_decode_name(nick,    out->nickname, sizeof(out->nickname));
}

static void gen1_encode_party_mon(const pksave_pokemon_t *in,
                                   uint8_t *data,
                                   uint8_t *ot_name,
                                   uint8_t *nick,
                                   uint16_t trainer_tid)
{
    /* Overlay exposed fields in-place so unsurfaced bytes (status@0x04,
     * types@0x05-0x06, catch_rate@0x07, OT ID@0x0C-0x0D, exp@0x0E-0x10)
     * survive round-trip edits.  Fresh/empty slots inherit trainer TID. */
    uint8_t  old_sp  = data[0x00];
    uint8_t  old_lvl = (data[0x21] != 0) ? data[0x21] : data[0x03];
    uint32_t old_xp  = ((uint32_t)data[0x0E] << 16)
                     | ((uint32_t)data[0x0F] << 8)
                     | (uint32_t)data[0x10];
    if (old_lvl == 0 && old_xp > 0) {
        old_lvl = pksave_level_from_xp(gen1_growth_rate_for_species(old_sp), old_xp);
    }

    if (old_sp == 0 && data[0x0C] == 0 && data[0x0D] == 0) {
        data[0x0C] = (uint8_t)(trainer_tid >> 8);
        data[0x0D] = (uint8_t)(trainer_tid & 0xFF);
    }

    /* Update types and catch_rate from base stats when species changes
     * (or on a fresh slot where types/catch_rate are zero). */
    if (((uint8_t)in->species != old_sp ||
         (data[0x05] == 0 && data[0x06] == 0 && data[0x07] == 0)) &&
        in->species < sizeof(gen1_base_stats) / sizeof(gen1_base_stats[0])) {
        const gen1_base_stats_t *bs = &gen1_base_stats[in->species];
        if (bs->hp != 0) {
            data[0x05] = bs->type1;
            data[0x06] = bs->type2;
            data[0x07] = bs->catch_rate;
        }
    }

    /* Recompute 3-byte experience when species or level changes (or on a
     * fresh slot), preserving within-level XP progress on no-op edits. */
    if ((uint8_t)in->species != old_sp || in->level != old_lvl || old_xp == 0) {
        uint32_t new_xp = pksave_xp_for_level(
            gen1_growth_rate_for_species(in->species), in->level);
        data[0x0E] = (uint8_t)(new_xp >> 16);
        data[0x0F] = (uint8_t)(new_xp >> 8);
        data[0x10] = (uint8_t)(new_xp & 0xFF);
    }

    data[0x00] = (uint8_t)in->species;
    data[0x21] = in->level;
    data[0x03] = in->level;   /* box_level field */

    for (int i = 0; i < 4; i++) {
        data[0x08 + i] = (uint8_t)in->move_ids[i];
        data[0x1D + i] = in->move_pp[i];
    }

    /* EVs → stat exp: store mapped values back */
    uint16_t se[5];
    se[0] = in->evs[0]; /* HP */
    se[1] = in->evs[1]; /* Atk */
    se[2] = in->evs[2]; /* Def */
    se[3] = in->evs[5]; /* Spe → Spd */
    se[4] = in->evs[3]; /* SpA → Spc (use SpA; ignore SpD for Gen I) */
    for (int i = 0; i < 5; i++) {
        data[0x11 + i*2] = (uint8_t)(se[i] >> 8);
        data[0x12 + i*2] = (uint8_t)(se[i] & 0xFF);
    }

    /* DVs: unscale from 0-31 back to 0-15 by halving */
    uint8_t dv_atk = (uint8_t)((in->ivs[1] / 2) & 0x0FU);
    uint8_t dv_def = (uint8_t)((in->ivs[2] / 2) & 0x0FU);
    uint8_t dv_spd = (uint8_t)((in->ivs[5] / 2) & 0x0FU);
    uint8_t dv_spc = (uint8_t)((in->ivs[3] / 2) & 0x0FU);
    data[0x1B] = (uint8_t)((dv_atk << 4) | dv_def);
    data[0x1C] = (uint8_t)((dv_spd << 4) | dv_spc);

    /* Stats */
    data[0x22] = (uint8_t)(in->stat[0] >> 8); data[0x23] = (uint8_t)in->stat[0];
    data[0x24] = (uint8_t)(in->stat[1] >> 8); data[0x25] = (uint8_t)in->stat[1];
    data[0x26] = (uint8_t)(in->stat[2] >> 8); data[0x27] = (uint8_t)in->stat[2];
    data[0x28] = (uint8_t)(in->stat[5] >> 8); data[0x29] = (uint8_t)in->stat[5];
    data[0x2A] = (uint8_t)(in->stat[3] >> 8); data[0x2B] = (uint8_t)in->stat[3];

    /* Current HP = max HP */
    data[0x01] = data[0x22]; data[0x02] = data[0x23];

    gen1_encode_name(in->ot_name,  ot_name);
    gen1_encode_name(in->nickname, nick);
}

/* ── box mon decode / encode ─────────────────────────────────────────────── */

static void gen1_decode_box_mon(const uint8_t *data,
                                 const uint8_t *ot_name,
                                 const uint8_t *nick,
                                 pksave_pokemon_t *out)
{
    /* Box mon (33 bytes) is like party mon but without level@0x21 and stats@0x22.
     * Level is at data[0x03] (box_level), falling back to XP@0x0E-0x10 if 0. */
    uint8_t party[G1_PARTY_MON_SIZE];
    memset(party, 0, G1_PARTY_MON_SIZE);
    memcpy(party, data, G1_BOX_MON_SIZE); /* copy 33 bytes */
    uint8_t lvl = data[0x03];
    if (lvl == 0) {
        uint32_t xp = ((uint32_t)data[0x0E] << 16)
                    | ((uint32_t)data[0x0F] << 8)
                    | (uint32_t)data[0x10];
        lvl = pksave_level_from_xp(gen1_growth_rate_for_species(data[0x00]), xp);
    }
    party[0x21] = lvl;
    gen1_decode_party_mon(party, ot_name, nick, out);
}

/* ── public genN API ─────────────────────────────────────────────────────── */

int gen1_get_trainer(pksave_save_t *s, pksave_trainer_t *out)
{
    gen1_decode_name(pksave_sram_cat(s->sram, G1_PLAYER_NAME),
                     out->name, sizeof(out->name));
    const uint8_t *tid_p = pksave_sram_cat(s->sram, G1_TID);
    out->tid    = (uint16_t)((tid_p[0] << 8) | tid_p[1]);
    out->sid    = 0;
    out->money  = bcd3_decode(pksave_sram_cat(s->sram, G1_MONEY));
    out->badges = *pksave_sram_cat(s->sram, G1_BADGES);
    return PKSAVE_OK;
}

int gen1_set_trainer(pksave_save_t *s, const pksave_trainer_t *in)
{
    gen1_encode_name(in->name, pksave_sram_at(s->sram, G1_PLAYER_NAME));
    uint8_t *tid_p = pksave_sram_at(s->sram, G1_TID);
    tid_p[0] = (uint8_t)(in->tid >> 8);
    tid_p[1] = (uint8_t)(in->tid & 0xFF);
    bcd3_encode(in->money, pksave_sram_at(s->sram, G1_MONEY));
    *pksave_sram_at(s->sram, G1_BADGES) = (uint8_t)in->badges;
    return PKSAVE_OK;
}

int gen1_get_party(pksave_save_t *s, pksave_pokemon_t out[6], uint8_t *count)
{
    uint8_t cnt = *pksave_sram_cat(s->sram, G1_PARTY_CNT);
    if (cnt > 6) cnt = 6;
    *count = cnt;
    const uint8_t *base_data  = pksave_sram_cat(s->sram, G1_PARTY_DATA);
    const uint8_t *base_ot    = pksave_sram_cat(s->sram, G1_PARTY_OT);
    const uint8_t *base_names = pksave_sram_cat(s->sram, G1_PARTY_NAMES);
    for (int i = 0; i < cnt; i++) {
        const uint8_t *data    = base_data  + (size_t)i * G1_PARTY_MON_SIZE;
        const uint8_t *ot_name = base_ot    + (size_t)i * G1_NAME_LEN;
        const uint8_t *nick    = base_names + (size_t)i * G1_NAME_LEN;
        gen1_decode_party_mon(data, ot_name, nick, &out[i]);
    }
    return PKSAVE_OK;
}

int gen1_set_party_slot(pksave_save_t *s, uint8_t slot, const pksave_pokemon_t *mon)
{
    if (slot >= 6) return PKSAVE_ERR_RANGE;
    uint8_t *cnt_p = pksave_sram_at(s->sram, G1_PARTY_CNT);
    uint8_t cnt = *cnt_p;
    const uint8_t *tid_p = pksave_sram_cat(s->sram, G1_TID);
    uint16_t trainer_tid = (uint16_t)((tid_p[0] << 8) | tid_p[1]);
    uint8_t *data    = pksave_sram_at(s->sram, G1_PARTY_DATA)  + (size_t)slot * G1_PARTY_MON_SIZE;
    uint8_t *ot_name = pksave_sram_at(s->sram, G1_PARTY_OT)    + (size_t)slot * G1_NAME_LEN;
    uint8_t *nick    = pksave_sram_at(s->sram, G1_PARTY_NAMES) + (size_t)slot * G1_NAME_LEN;
    gen1_encode_party_mon(mon, data, ot_name, nick, trainer_tid);
    /* Update species list */
    uint8_t *spec_p = pksave_sram_at(s->sram, G1_PARTY_SPEC);
    spec_p[slot] = (uint8_t)mon->species;
    if (slot >= cnt) {
        *cnt_p = slot + 1;
        spec_p[slot + 1] = 0xFF;
    }
    return PKSAVE_OK;
}

int gen1_get_box(pksave_save_t *s, uint8_t box, pksave_pokemon_t out[30], uint8_t *cnt)
{
    if (box >= 12) return PKSAVE_ERR_RANGE;

    uint8_t cur_box = *pksave_sram_cat(s->sram, G1_CUR_BOX) & 0x7FU;
    const uint8_t *base;
    if (box == cur_box) {
        /* Current box stored in bank 1 */
        base = pksave_sram_cat(s->sram, G1_CUR_BOX_DATA);
    } else {
        base = pksave_sram_cat(s->sram, G1_BOX_OFFSET(box));
    }

    uint8_t n = base[0];
    if (n > 20) n = 20;
    *cnt = n;

    /* Box layout: 1 count + 21 species + 20×33 data + 20×11 OT + 20×11 nick */
    const uint8_t *species = base + 1;
    const uint8_t *data    = base + 22;                    /* 1+21 */
    const uint8_t *ot      = data + 20 * G1_BOX_MON_SIZE; /* +660 */
    const uint8_t *nicks   = ot   + 20 * G1_NAME_LEN;     /* +220 */

    for (int i = 0; i < n; i++) {
        gen1_decode_box_mon(data + (size_t)i * G1_BOX_MON_SIZE,
                             ot   + (size_t)i * G1_NAME_LEN,
                             nicks+ (size_t)i * G1_NAME_LEN,
                             &out[i]);
        out[i].species = species[i]; /* override from species list */
    }
    return PKSAVE_OK;
}

int gen1_set_box_slot(pksave_save_t *s, uint8_t box, uint8_t slot,
                       const pksave_pokemon_t *mon)
{
    if (box >= 12 || slot >= 20) return PKSAVE_ERR_RANGE;

    const uint8_t *tid_p = pksave_sram_cat(s->sram, G1_TID);
    uint16_t trainer_tid = (uint16_t)((tid_p[0] << 8) | tid_p[1]);
    uint8_t *cur_box_p = pksave_sram_at(s->sram, G1_CUR_BOX);
    uint8_t cur_box = *cur_box_p & 0x7FU;
    uint8_t *base;
    if (box == cur_box) {
        base = pksave_sram_at(s->sram, G1_CUR_BOX_DATA);
    } else {
        /* Mark BIT_HAS_CHANGED_BOXES (bit 7) so the cartridge knows Banks 2/3
         * contain initialized boxes and does not wipe them on first box switch. */
        *cur_box_p |= 0x80U;
        base = pksave_sram_at(s->sram, G1_BOX_OFFSET(box));
    }

    uint8_t *data  = base + 22 + (size_t)slot * G1_BOX_MON_SIZE;
    uint8_t *ot    = base + 22 + 20 * G1_BOX_MON_SIZE + (size_t)slot * G1_NAME_LEN;
    uint8_t *nick  = base + 22 + 20 * G1_BOX_MON_SIZE + 20 * G1_NAME_LEN
                               + (size_t)slot * G1_NAME_LEN;

    /* Seed tmp from existing 33-byte box mon so unsurfaced fields survive */
    uint8_t tmp[G1_PARTY_MON_SIZE];
    uint8_t ot_tmp[G1_NAME_LEN], nick_tmp[G1_NAME_LEN];
    memset(tmp, 0, sizeof(tmp));
    memcpy(tmp, data, G1_BOX_MON_SIZE);
    gen1_encode_party_mon(mon, tmp, ot_tmp, nick_tmp, trainer_tid);
    memcpy(data, tmp, G1_BOX_MON_SIZE);
    memcpy(ot, ot_tmp, G1_NAME_LEN);
    memcpy(nick, nick_tmp, G1_NAME_LEN);

    base[1 + slot] = (uint8_t)mon->species; /* update species list */
    if (slot >= base[0]) {
        base[0] = slot + 1;
        base[1 + slot + 1] = 0xFF;
    }
    return PKSAVE_OK;
}

/* Gen I bag: single ITEMS pocket only. count byte + pairs (id, qty). */
int gen1_get_pocket(pksave_save_t *s, pksave_pocket_t p, pksave_item_t *out, uint8_t *cnt)
{
    if (p != PKSAVE_POCKET_ITEMS) {
        *cnt = 0;
        return PKSAVE_OK;
    }
    uint8_t n = *pksave_sram_cat(s->sram, G1_BAG_CNT);
    if (n > G1_BAG_MAX) n = G1_BAG_MAX;
    *cnt = n;
    const uint8_t *bag = pksave_sram_cat(s->sram, G1_BAG_DATA);
    for (int i = 0; i < n; i++) {
        out[i].item_id  = bag[i * 2];
        out[i].quantity = bag[i * 2 + 1];
    }
    return PKSAVE_OK;
}

int gen1_set_item(pksave_save_t *s, pksave_pocket_t p, uint8_t slot,
                   const pksave_item_t *item)
{
    if (p != PKSAVE_POCKET_ITEMS) return PKSAVE_ERR_GAME;
    if (slot >= G1_BAG_MAX) return PKSAVE_ERR_RANGE;
    uint8_t *bag = pksave_sram_at(s->sram, G1_BAG_DATA);
    bag[slot * 2]     = (uint8_t)item->item_id;
    bag[slot * 2 + 1] = (uint8_t)item->quantity;
    uint8_t *cnt_p = pksave_sram_at(s->sram, G1_BAG_CNT);
    if (slot >= *cnt_p)
        *cnt_p = slot + 1;
    return PKSAVE_OK;
}

int gen1_flush(pksave_save_t *s)
{
    uint8_t chk = gen1_calc_checksum(s->sram);
    *pksave_sram_at(s->sram, G1_CHKSUM_ADDR) = chk;
    gen1_update_box_bank_checksums(s->sram, 0x4000U);
    gen1_update_box_bank_checksums(s->sram, 0x6000U);
    return PKSAVE_OK;
}

#endif /* PKSAVE_GEN1 */
