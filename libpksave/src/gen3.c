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

#include "names/gen3_growth_rates.h"

/* ── Gen 3 experience curves (per pret/pokeemerald
 *    src/data/pokemon/experience_tables.h) ───────────────────────────────── */
/* Six growth-rate curves, indexed to match gen3_growth_rates[].
 *   0 = MEDIUM_FAST   1 = ERRATIC     2 = FLUCTUATING
 *   3 = MEDIUM_SLOW   4 = FAST        5 = SLOW
 *
 * Formulas computed at runtime (rather than precomputed 6×101 = 606-entry
 * table) — the math is integer-only and called at most once per slot
 * read/write.  Pokemon's "level 1" XP is canonically 1 in pret's tables,
 * but the encoded formula at n=1 yields 0/1/<0> across the rates; we
 * canonicalise: n<=1 → 0.  Decode treats both 0 and 1 as level 1. */
static uint32_t g3_xp_for_level(uint8_t rate, uint8_t lvl)
{
    if (lvl <= 1) return 0;
    uint32_t n  = (uint32_t)lvl;
    uint32_t n2 = n * n;
    uint32_t n3 = n2 * n;
    switch (rate) {
    case 0: /* MEDIUM_FAST */
        return n3;
    case 1: /* ERRATIC */
        if (n <= 50) return ((100u - n) * n3) /  50u;
        if (n <= 68) return ((150u - n) * n3) / 100u;
        if (n <= 98) return (((1911u - 10u * n) / 3u) * n3) / 500u;
        return ((160u - n) * n3) / 100u;
    case 2: /* FLUCTUATING */
        if (n <= 15) return ((((n + 1u) / 3u) + 24u) * n3) /  50u;
        if (n <= 36) return ((n + 14u)                * n3) /  50u;
        return (((n / 2u) + 32u)                      * n3) /  50u;
    case 3: /* MEDIUM_SLOW */
        /* (6n^3)/5 - 15n^2 + 100n - 140; positive for n >= 2. */
        return (6u * n3) / 5u + 100u * n - 15u * n2 - 140u;
    case 4: /* FAST */
        return (4u * n3) / 5u;
    case 5: /* SLOW */
        return (5u * n3) / 4u;
    default:
        return n3;
    }
}

static uint8_t g3_level_from_xp(uint8_t rate, uint32_t xp)
{
    /* Scan down from 100; first level whose threshold is <= xp wins.
     * Capped at 100, floored at 1 — Gen 3 has no level 0. */
    uint8_t lvl = 100;
    while (lvl > 1 && g3_xp_for_level(rate, lvl) > xp) {
        --lvl;
    }
    return lvl;
}

static uint8_t g3_growth_rate_for_species(uint16_t species_id)
{
    if (species_id < sizeof(gen3_growth_rates) / sizeof(gen3_growth_rates[0])) {
        return gen3_growth_rates[species_id];
    }
    return 0; /* MEDIUM_FAST as a safe default */
}

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

/* Gen III character map (per pret/pokeemerald charmap.txt):
 *   0x00         = space
 *   0xA1..0xAA   = '0'..'9'
 *   0xAB..0xBA   = punctuation (! ? . - · … " " ' ' ♂ ♀ $ , × /)
 *   0xBB..0xD4   = 'A'..'Z'
 *   0xD5..0xEE   = 'a'..'z'
 *   0xFF         = end-of-string terminator
 * Bytes the cartridge maps to glyphs we don't surface decode to '?';
 * ASCII the encoder can't represent maps to 0xFF (terminator). */
static char gen3_decode_char(uint8_t b)
{
    if (b == 0xFF) return '\0';
    if (b == 0x00) return ' ';
    if (b >= 0xA1 && b <= 0xAA) return (char)('0' + (b - 0xA1));
    if (b >= 0xBB && b <= 0xD4) return (char)('A' + (b - 0xBB));
    if (b >= 0xD5 && b <= 0xEE) return (char)('a' + (b - 0xD5));
    switch (b) {
        case 0xAB: return '!';
        case 0xAC: return '?';
        case 0xAD: return '.';
        case 0xAE: return '-';
        case 0xB1: return '"';
        case 0xB4: return '\'';
        case 0xB8: return ',';
        case 0xBA: return '/';
        default:   return '?';
    }
}

static uint8_t gen3_encode_char(char c)
{
    if (c == '\0') return 0xFF;
    if (c == ' ')  return 0x00;
    if (c >= '0' && c <= '9') return (uint8_t)(0xA1 + (c - '0'));
    if (c >= 'A' && c <= 'Z') return (uint8_t)(0xBB + (c - 'A'));
    if (c >= 'a' && c <= 'z') return (uint8_t)(0xD5 + (c - 'a'));
    switch (c) {
        case '!':  return 0xAB;
        case '?':  return 0xAC;
        case '.':  return 0xAD;
        case '-':  return 0xAE;
        case '"':  return 0xB1;
        case '\'': return 0xB4;
        case ',':  return 0xB8;
        case '/':  return 0xBA;
        default:   return 0xFF;
    }
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

    /* Misc IVs: bits 0-4=HP, 5-9=Atk, 10-14=Def, 15-19=Spd, 20-24=SpA, 25-29=SpD;
     * bit 30 is the egg flag; bit 31 is the ability slot (0 or 1). */
    uint32_t iv; memcpy(&iv, dec + mp, 4);
    out->ivs[0] = (uint8_t)((iv)      & 0x1F);
    out->ivs[1] = (uint8_t)((iv >> 5) & 0x1F);
    out->ivs[2] = (uint8_t)((iv >>10) & 0x1F);
    out->ivs[5] = (uint8_t)((iv >>15) & 0x1F);
    out->ivs[3] = (uint8_t)((iv >>20) & 0x1F);
    out->ivs[4] = (uint8_t)((iv >>25) & 0x1F);
    out->ability = (uint8_t)((iv >> 31) & 1U);

    /* Nature is PID % 25; shininess is (low^high^TID^SID) < 8 per Gen 3 rule. */
    out->nature   = (uint8_t)(pid % 25U);
    out->is_shiny = (uint8_t)(((((pid & 0xFFFFU) ^ (pid >> 16)) ^ tid ^ sid) < 8U) ? 1 : 0);

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
/* Walk PIDs in steps of 25 (preserves nature == pid % 25) starting from
 * a hint, returning the first PID whose shininess matches target_shiny.
 * Expected iterations: ~1 for non-shiny, ~8192 for shiny.  Caps at 100k
 * for safety; on cap-hit returns -1 and caller keeps the old PID. */
static int g3_find_pid(uint8_t target_nature, uint8_t target_shiny,
                       uint16_t tid, uint16_t sid, uint32_t hint,
                       uint32_t *out_pid)
{
    target_nature = (uint8_t)(target_nature % 25U);
    target_shiny  = (uint8_t)(target_shiny & 1U);

    /* Anchor base so base % 25 == target_nature. */
    uint32_t base = hint - (hint % 25U) + target_nature;

    for (uint32_t step = 0; step < 100000U; step++) {
        uint32_t pid = base + step * 25U;
        if (pid == 0) continue;  /* PID=0 is the box-decoder's empty sentinel */
        uint8_t shiny = (uint8_t)(((((pid & 0xFFFFU) ^ (pid >> 16)) ^ tid ^ sid) < 8U) ? 1 : 0);
        if (shiny == target_shiny) {
            *out_pid = pid;
            return 0;
        }
    }
    return -1;
}

/* Encode the 80-byte encrypted block (PID/TID/SID, names, encrypted
 * substructs, checksum).  Shared by g3_encode_mon (party) and
 * g3_encode_box_mon (box); the party encoder calls it then writes its
 * own level/stats extras at 0x50+.
 *
 * Handles PID recompute when caller's mon->nature / mon->is_shiny diverge
 * from the existing on-disk PID — searches for a satisfying PID, then
 * re-arranges the substructs in the cleartext block to match the new
 * pid%24 ordering before encrypting.  Caller-supplied fields (species,
 * EVs, IVs, moves, ability slot) overlay the existing substructs;
 * unsurfaced bytes (contest stats, friendship, met data, etc.) survive. */
static void g3_encode_encrypted_block(uint8_t *dst, const pksave_pokemon_t *mon,
                                       uint16_t trainer_tid, uint16_t trainer_sid)
{
    uint32_t pid; memcpy(&pid, dst, 4);
    uint16_t tid; memcpy(&tid, dst + 4, 2);
    uint16_t sid; memcpy(&sid, dst + 6, 2);

    /* Fresh slot: inherit trainer IDs.  Box-fresh-slot (pid also 0)
     * synthesizes a starter PID so the slot decodes as non-empty. */
    if (tid == 0 && sid == 0) {
        tid = trainer_tid;
        sid = trainer_sid;
        memcpy(dst + 4, &tid, 2);
        memcpy(dst + 6, &sid, 2);
        if (pid == 0) {
            pid = 0x12345678u;
            memcpy(dst, &pid, 4);
        }
    }

    /* Decrypt with the OLD pid-derived key, pull substructs out so we can
     * lay them down in a new order if the PID changes. */
    uint32_t key = pid ^ ((uint32_t)tid | ((uint32_t)sid << 16));
    uint8_t dec[48];
    g3_crypt(dst + 0x20, key, dec);

    uint8_t subs[4][12];
    const uint8_t *old_ord = g3_suborder[pid % 24];
    for (int i = 0; i < 4; i++) memcpy(subs[old_ord[i]], dec + i*12, 12);

    /* PID swap when caller asked for a nature/shiny that the current PID
     * can't represent.  Re-derive the encryption key from the new PID. */
    uint8_t curr_shiny  = (uint8_t)(((((pid & 0xFFFFU) ^ (pid >> 16)) ^ tid ^ sid) < 8U) ? 1 : 0);
    uint8_t curr_nature = (uint8_t)(pid % 25U);
    if ((mon->nature % 25U) != curr_nature || (mon->is_shiny & 1U) != curr_shiny) {
        uint32_t new_pid;
        if (g3_find_pid(mon->nature, mon->is_shiny, tid, sid, pid, &new_pid) == 0) {
            pid = new_pid;
            memcpy(dst, &pid, 4);
            key = pid ^ ((uint32_t)tid | ((uint32_t)sid << 16));
        }
        /* Search failure (100k iterations without a match) leaves the old
         * PID in place — caller's shiny/nature change is silently dropped
         * rather than corrupting the slot.  In practice the cap is well
         * beyond what's ever needed. */
    }

    /* Overlay caller's fields onto the typed substructs.
     * subs[0]=Growth, subs[1]=Attacks, subs[2]=EVs, subs[3]=Misc. */
    uint8_t *g = subs[0], *a = subs[1], *e = subs[2], *m = subs[3];

    /* Capture pre-overlay species + XP so the level↔XP encoder below can
     * decide whether to preserve the existing XP (no-op edit) or rewrite
     * it to the threshold for caller-supplied mon->level. */
    uint16_t old_sp; memcpy(&old_sp, g + 0, 2);
    uint32_t old_xp; memcpy(&old_xp, g + 4, 4);

    uint16_t sp = mon->species;   memcpy(g + 0, &sp, 2);
    uint16_t it = mon->held_item; memcpy(g + 2, &it, 2);

    /* XP: Gen 3 stores experience (not level) in the Growth substruct at
     * +4 (4 bytes LE; cartridge writes only the low 3 bytes, top byte
     * canonically zero).  Level on read is derived from XP via the
     * species's growth-rate curve, so an editor change to mon->level
     * must round-trip into a corresponding XP write.
     *
     * Preserve XP exactly when both species and level are unchanged from
     * what's currently encoded — that keeps within-level XP progress
     * across no-op edits (e.g. user opened the slot, walked through
     * tabs, hit Save without touching level).  When either changes,
     * rewrite XP to the new species's growth-rate threshold for
     * mon->level.  This loses sub-level progress but is the predictable
     * editor semantic ("the level you typed is the level you get"). */
    uint8_t old_level = g3_level_from_xp(g3_growth_rate_for_species(old_sp),
                                          old_xp);
    uint32_t new_xp;
    if (mon->level == old_level && mon->species == old_sp) {
        new_xp = old_xp;
    } else {
        new_xp = g3_xp_for_level(g3_growth_rate_for_species(mon->species),
                                  mon->level);
    }
    memcpy(g + 4, &new_xp, 4);

    for (int i = 0; i < 4; i++) {
        uint16_t mv = mon->move_ids[i];
        memcpy(a + i*2, &mv, 2);
        a[8 + i] = mon->move_pp[i];
    }

    /* EVs at e+0..5: on-disk order HP,Atk,Def,Spd,SpA,SpD; struct order
     * HP,Atk,Def,SpA,SpD,Spe — so Spd<->SpA/SpD shuffle. */
    e[0] = (uint8_t)mon->evs[0];
    e[1] = (uint8_t)mon->evs[1];
    e[2] = (uint8_t)mon->evs[2];
    e[3] = (uint8_t)mon->evs[5];
    e[4] = (uint8_t)mon->evs[3];
    e[5] = (uint8_t)mon->evs[4];

    /* IV word: bits 0-29 IVs, bit 30 egg flag (preserved), bit 31
     * ability slot (now writable via mon->ability). */
    uint32_t iv; memcpy(&iv, m, 4);
    uint32_t egg = iv & 0x40000000u;
    iv = egg
       | ((uint32_t)(mon->ivs[0] & 0x1F))
       | ((uint32_t)(mon->ivs[1] & 0x1F) << 5)
       | ((uint32_t)(mon->ivs[2] & 0x1F) << 10)
       | ((uint32_t)(mon->ivs[5] & 0x1F) << 15)
       | ((uint32_t)(mon->ivs[3] & 0x1F) << 20)
       | ((uint32_t)(mon->ivs[4] & 0x1F) << 25)
       | ((uint32_t)(mon->ability & 1U) << 31);
    memcpy(m, &iv, 4);

    /* Lay substructs down at the NEW pid's ordering. */
    const uint8_t *new_ord = g3_suborder[pid % 24];
    for (int i = 0; i < 4; i++) memcpy(dec + i*12, subs[new_ord[i]], 12);

    /* Checksum = 16-bit sum of uint16 words in unencrypted substructs */
    uint32_t sum = 0;
    for (int i = 0; i < 48; i += 2) {
        uint16_t w; memcpy(&w, dec + i, 2);
        sum += w;
    }
    uint16_t checksum = (uint16_t)sum;
    memcpy(dst + 0x1C, &checksum, 2);

    g3_crypt(dec, key, dst + 0x20);

    gen3_encode_name(mon->ot_name,  dst + 0x14, 7);
    gen3_encode_name(mon->nickname, dst + 0x08, 10);
}

static void g3_encode_mon(uint8_t *dst, const pksave_pokemon_t *mon,
                          uint16_t trainer_tid, uint16_t trainer_sid)
{
    g3_encode_encrypted_block(dst, mon, trainer_tid, trainer_sid);

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

/* PC storage spans sections 5..13 as a single linear byte array:
 *   { u8 currentBox; BoxPokemon boxes[14][30]; u8 boxNames[14][9];
 *     u8 boxWallpapers[14]; }   (33741 bytes total)
 * Sections 5..12 each contribute 3968 data bytes; section 13 contributes
 * 2000 — together 33744 bytes of capacity. */
#define G3_PC_FIRST_SEC      5U
#define G3_PC_LAST_SEC       13U
#define G3_PC_SEC_SIZE       3968U
#define G3_PC_SEC13_SIZE     2000U
#define G3_PC_REGULAR_TOTAL  (8U * G3_PC_SEC_SIZE)   /* sections 5..12 */

#define G3_SLOTS_PER_BOX     30U
#define G3_NUM_BOXES         14U

static int g3_pc_read(const uint8_t *sram, uint32_t slot_off,
                       uint32_t lin_off, uint8_t *dst, size_t len)
{
    while (len > 0) {
        uint16_t sec_idx;
        uint32_t within, sec_data_size;
        if (lin_off < G3_PC_REGULAR_TOTAL) {
            sec_idx = (uint16_t)(G3_PC_FIRST_SEC + lin_off / G3_PC_SEC_SIZE);
            within  = lin_off % G3_PC_SEC_SIZE;
            sec_data_size = G3_PC_SEC_SIZE;
        } else {
            sec_idx = G3_PC_LAST_SEC;
            within  = lin_off - G3_PC_REGULAR_TOTAL;
            sec_data_size = G3_PC_SEC13_SIZE;
        }
        size_t sec_off = g3_find_section(sram, slot_off, sec_idx);
        if (sec_off == SIZE_MAX) return PKSAVE_ERR_CORRUPT;
        uint32_t avail = sec_data_size - within;
        size_t take = (len < avail) ? len : avail;
        memcpy(dst, sram + sec_off + within, take);
        dst    += take;
        lin_off += (uint32_t)take;
        len    -= take;
    }
    return PKSAVE_OK;
}

/* Box mon (80 bytes): same layout as the encrypted portion of a party mon,
 * without the party extras at 0x50+ (level/stats). */
static void g3_decode_box_mon(const uint8_t *mon, pksave_pokemon_t *out)
{
    memset(out, 0, sizeof(*out));

    uint32_t pid; memcpy(&pid, mon, 4);
    uint16_t tid; memcpy(&tid, mon + 4, 2);
    uint16_t sid; memcpy(&sid, mon + 6, 2);

    if (pid == 0 && tid == 0 && sid == 0) return;   /* empty slot */

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
    out->ability = (uint8_t)((iv >> 31) & 1U);

    out->nature   = (uint8_t)(pid % 25U);
    out->is_shiny = (uint8_t)(((((pid & 0xFFFFU) ^ (pid >> 16)) ^ tid ^ sid) < 8U) ? 1 : 0);

    gen3_decode_name(mon + 0x14, out->ot_name,  sizeof(out->ot_name),  7);
    gen3_decode_name(mon + 0x08, out->nickname, sizeof(out->nickname), 10);

    /* Derive level from XP: box mons store experience (3 bytes LE at the
     * Growth substruct + 4; the 4th byte is canonically zero and we
     * tolerate non-zero by reading the full uint32) and the cartridge
     * recomputes level from XP at withdrawal time using the species's
     * growth-rate curve.  We do the same so the UI sees a meaningful
     * level value rather than 0.  stat[6] stays zero (callers can
     * recompute via pksave_recalc_stats if they need it). */
    uint32_t xp; memcpy(&xp, dec + gp + 4, 4);
    out->level = g3_level_from_xp(g3_growth_rate_for_species(out->species), xp);
}

int gen3_get_box(pksave_save_t *s, uint8_t box, pksave_pokemon_t out[30], uint8_t *cnt)
{
    if (box >= G3_NUM_BOXES) return PKSAVE_ERR_RANGE;
    *cnt = 0;
    uint32_t slot_off = g3_active_slot(s->sram);

    for (uint8_t slot = 0; slot < G3_SLOTS_PER_BOX; slot++) {
        uint8_t buf[G3_BOX_MON_SIZE];
        uint32_t lin = 1U
                     + (uint32_t)box  * G3_SLOTS_PER_BOX * G3_BOX_MON_SIZE
                     + (uint32_t)slot * G3_BOX_MON_SIZE;
        int rc = g3_pc_read(s->sram, slot_off, lin, buf, G3_BOX_MON_SIZE);
        if (rc != PKSAVE_OK) return rc;
        g3_decode_box_mon(buf, &out[slot]);
    }
    *cnt = G3_SLOTS_PER_BOX;
    return PKSAVE_OK;
}

/* Mirror of g3_pc_read: write `src` bytes into the linear PC buffer starting
 * at `lin_off`, splitting across sections 5..13. */
static int g3_pc_write(uint8_t *sram, uint32_t slot_off,
                        uint32_t lin_off, const uint8_t *src, size_t len)
{
    while (len > 0) {
        uint16_t sec_idx;
        uint32_t within, sec_data_size;
        if (lin_off < G3_PC_REGULAR_TOTAL) {
            sec_idx = (uint16_t)(G3_PC_FIRST_SEC + lin_off / G3_PC_SEC_SIZE);
            within  = lin_off % G3_PC_SEC_SIZE;
            sec_data_size = G3_PC_SEC_SIZE;
        } else {
            sec_idx = G3_PC_LAST_SEC;
            within  = lin_off - G3_PC_REGULAR_TOTAL;
            sec_data_size = G3_PC_SEC13_SIZE;
        }
        size_t sec_off = g3_find_section(sram, slot_off, sec_idx);
        if (sec_off == SIZE_MAX) return PKSAVE_ERR_CORRUPT;
        uint32_t avail = sec_data_size - within;
        size_t put = (len < avail) ? len : avail;
        memcpy(sram + sec_off + within, src, put);
        src     += put;
        lin_off += (uint32_t)put;
        len     -= put;
    }
    return PKSAVE_OK;
}

/* Encode a Pokemon into the 80-byte box-mon layout (no party extras).
 * Shares the encrypted-block encoder with g3_encode_mon, including the
 * PID-recompute logic for shiny / nature changes. */
static void g3_encode_box_mon(uint8_t *dst, const pksave_pokemon_t *mon,
                               uint16_t trainer_tid, uint16_t trainer_sid)
{
    g3_encode_encrypted_block(dst, mon, trainer_tid, trainer_sid);
}

int gen3_set_box_slot(pksave_save_t *s, uint8_t box, uint8_t slot,
                       const pksave_pokemon_t *mon)
{
    if (box >= G3_NUM_BOXES || slot >= G3_SLOTS_PER_BOX) return PKSAVE_ERR_RANGE;

    uint32_t slot_off = g3_active_slot(s->sram);
    size_t s0 = g3_find_section(s->sram, slot_off, 0);
    if (s0 == SIZE_MAX) return PKSAVE_ERR_CORRUPT;

    uint16_t trainer_tid, trainer_sid;
    memcpy(&trainer_tid, s->sram + s0 + 0x00A, 2);
    memcpy(&trainer_sid, s->sram + s0 + 0x00C, 2);

    uint32_t lin = 1U
                 + (uint32_t)box  * G3_SLOTS_PER_BOX * G3_BOX_MON_SIZE
                 + (uint32_t)slot * G3_BOX_MON_SIZE;

    /* Read the existing 80 bytes so the encode pass can preserve untouched
     * fields (exp, friendship, contest stats, met data, egg/nickname flags). */
    uint8_t buf[G3_BOX_MON_SIZE];
    int rc = g3_pc_read(s->sram, slot_off, lin, buf, G3_BOX_MON_SIZE);
    if (rc != PKSAVE_OK) return rc;

    g3_encode_box_mon(buf, mon, trainer_tid, trainer_sid);

    return g3_pc_write(s->sram, slot_off, lin, buf, G3_BOX_MON_SIZE);
}

/* ── pokerus ─────────────────────────────────────────────────────────────── */
/* Stored as byte 0 of the Misc substruct (M=3 in g3_suborder). Substructs
 * are encrypted with key = PID XOR (TID|SID<<16), and the per-mon checksum
 * at offset 0x1C must be recomputed after any write. */

static int g3_substruct_offsets(const uint8_t *mon, uint32_t *out_key,
                                 int *out_mp)
{
    uint32_t pid; memcpy(&pid, mon, 4);
    uint16_t tid; memcpy(&tid, mon + 4, 2);
    uint16_t sid; memcpy(&sid, mon + 6, 2);
    if (pid == 0 && tid == 0 && sid == 0) return PKSAVE_ERR_RANGE; /* empty */
    *out_key = pid ^ ((uint32_t)tid | ((uint32_t)sid << 16));
    const uint8_t *ord = g3_suborder[pid % 24];
    for (int i = 0; i < 4; i++) {
        if (ord[i] == 3) { *out_mp = i * 12; return PKSAVE_OK; }
    }
    return PKSAVE_ERR_CORRUPT;
}

static int g3_pokerus_read(const uint8_t *mon, pksave_pokerus_t *out)
{
    uint32_t key; int mp;
    int rc = g3_substruct_offsets(mon, &key, &mp);
    if (rc != PKSAVE_OK) return rc;
    uint8_t dec[48];
    g3_crypt(mon + 0x20, key, dec);
    pksave_pokerus_decode(dec[mp + 0], out);
    return PKSAVE_OK;
}

static int g3_pokerus_write(uint8_t *mon, uint8_t strain, uint8_t days)
{
    uint32_t key; int mp;
    int rc = g3_substruct_offsets(mon, &key, &mp);
    if (rc != PKSAVE_OK) return rc;
    uint8_t dec[48];
    g3_crypt(mon + 0x20, key, dec);
    dec[mp + 0] = pksave_pokerus_encode(strain, days);
    uint32_t sum = 0;
    for (int i = 0; i < 48; i += 2) {
        uint16_t w; memcpy(&w, dec + i, 2);
        sum += w;
    }
    uint16_t checksum = (uint16_t)sum;
    memcpy(mon + 0x1C, &checksum, 2);
    g3_crypt(dec, key, mon + 0x20);
    return PKSAVE_OK;
}

int gen3_get_party_pokerus(pksave_save_t *s, uint8_t slot, pksave_pokerus_t *out)
{
    uint32_t slot_off = g3_active_slot(s->sram);
    size_t s1 = g3_find_section(s->sram, slot_off, 1);
    if (s1 == SIZE_MAX) return PKSAVE_ERR_CORRUPT;
    const uint8_t *sec1 = s->sram + s1;
    uint32_t cnt;
    memcpy(&cnt, sec1 + g3_team_cnt_off(s->game), 4);
    if (slot >= cnt) return PKSAVE_ERR_RANGE;
    const uint8_t *mon = sec1 + g3_team_data_off(s->game)
                       + (size_t)slot * G3_MON_SIZE;
    return g3_pokerus_read(mon, out);
}

int gen3_set_party_pokerus(pksave_save_t *s, uint8_t slot,
                            uint8_t strain, uint8_t days)
{
    uint32_t slot_off = g3_active_slot(s->sram);
    size_t s1 = g3_find_section(s->sram, slot_off, 1);
    if (s1 == SIZE_MAX) return PKSAVE_ERR_CORRUPT;
    uint8_t *sec1 = s->sram + s1;
    uint32_t cnt;
    memcpy(&cnt, sec1 + g3_team_cnt_off(s->game), 4);
    if (slot >= cnt) return PKSAVE_ERR_RANGE;
    uint8_t *mon = sec1 + g3_team_data_off(s->game)
                 + (size_t)slot * G3_MON_SIZE;
    return g3_pokerus_write(mon, strain, days);
}

int gen3_get_box_pokerus(pksave_save_t *s, uint8_t box, uint8_t slot,
                          pksave_pokerus_t *out)
{
    if (box >= G3_NUM_BOXES) return PKSAVE_ERR_RANGE;
    uint32_t slot_off = g3_active_slot(s->sram);
    uint8_t buf[G3_BOX_MON_SIZE];
    uint32_t lin = 1U
                 + (uint32_t)box  * G3_SLOTS_PER_BOX * G3_BOX_MON_SIZE
                 + (uint32_t)slot * G3_BOX_MON_SIZE;
    int rc = g3_pc_read(s->sram, slot_off, lin, buf, G3_BOX_MON_SIZE);
    if (rc != PKSAVE_OK) return rc;
    return g3_pokerus_read(buf, out);
}

int gen3_set_box_pokerus(pksave_save_t *s, uint8_t box, uint8_t slot,
                          uint8_t strain, uint8_t days)
{
    if (box >= G3_NUM_BOXES) return PKSAVE_ERR_RANGE;
    uint32_t slot_off = g3_active_slot(s->sram);
    uint8_t buf[G3_BOX_MON_SIZE];
    uint32_t lin = 1U
                 + (uint32_t)box  * G3_SLOTS_PER_BOX * G3_BOX_MON_SIZE
                 + (uint32_t)slot * G3_BOX_MON_SIZE;
    int rc = g3_pc_read(s->sram, slot_off, lin, buf, G3_BOX_MON_SIZE);
    if (rc != PKSAVE_OK) return rc;
    rc = g3_pokerus_write(buf, strain, days);
    if (rc != PKSAVE_OK) return rc;
    return g3_pc_write(s->sram, slot_off, lin, buf, G3_BOX_MON_SIZE);
}

/* ── bag / item pockets ──────────────────────────────────────────────────── */
/* Gen III item slot = u16 id + u16 quantity, quantity XORed with low 16 bits
 * of the section-0 security key. (RS key = 0 → no-op; FRLG key from sec0+0x0AF8.)
 * Pockets sit inside section 1 (SaveBlock1) at game-specific offsets.        */

struct g3_pocket_layout {
    uint16_t off;        /* offset within section 1 data */
    uint8_t  max_slots;
};

static int g3_pocket_layout(pksave_game_t game, pksave_pocket_t p,
                            struct g3_pocket_layout *out)
{
    if (game == PKSAVE_GAME_FRLG) {
        switch (p) {
            case PKSAVE_POCKET_ITEMS:   out->off = 0x310; out->max_slots = 42; return PKSAVE_OK;
            case PKSAVE_POCKET_KEY:     out->off = 0x3B8; out->max_slots = 30; return PKSAVE_OK;
            case PKSAVE_POCKET_BALLS:   out->off = 0x430; out->max_slots = 13; return PKSAVE_OK;
            case PKSAVE_POCKET_TM_HM:   out->off = 0x464; out->max_slots = 58; return PKSAVE_OK;
            case PKSAVE_POCKET_BERRIES: out->off = 0x54C; out->max_slots = 43; return PKSAVE_OK;
            default: return PKSAVE_ERR_GAME;
        }
    }
    /* RS / Emerald (RS-layout; matches the fixture generator) */
    switch (p) {
        case PKSAVE_POCKET_ITEMS:   out->off = 0x560; out->max_slots = 20; return PKSAVE_OK;
        case PKSAVE_POCKET_KEY:     out->off = 0x5B0; out->max_slots = 20; return PKSAVE_OK;
        case PKSAVE_POCKET_BALLS:   out->off = 0x600; out->max_slots = 16; return PKSAVE_OK;
        case PKSAVE_POCKET_TM_HM:   out->off = 0x640; out->max_slots = 64; return PKSAVE_OK;
        case PKSAVE_POCKET_BERRIES: out->off = 0x740; out->max_slots = 46; return PKSAVE_OK;
        default: return PKSAVE_ERR_GAME;
    }
}

int gen3_get_pocket(pksave_save_t *s, pksave_pocket_t p, pksave_item_t *out, uint8_t *cnt)
{
    struct g3_pocket_layout layout;
    if (g3_pocket_layout(s->game, p, &layout) != PKSAVE_OK) {
        *cnt = 0;
        return PKSAVE_OK;
    }

    uint32_t slot_off = g3_active_slot(s->sram);
    size_t s0 = g3_find_section(s->sram, slot_off, 0);
    size_t s1 = g3_find_section(s->sram, slot_off, 1);
    if (s0 == SIZE_MAX || s1 == SIZE_MAX) return PKSAVE_ERR_CORRUPT;

    const uint8_t *sec0 = s->sram + s0;
    const uint8_t *sec1 = s->sram + s1;
    uint16_t qty_key = (uint16_t)(g3_security_key(sec0, s->game) & 0xFFFF);

    uint8_t n = 0;
    for (uint8_t i = 0; i < layout.max_slots; i++) {
        const uint8_t *entry = sec1 + layout.off + (size_t)i * 4;
        uint16_t id, qty;
        memcpy(&id,  entry,     2);
        memcpy(&qty, entry + 2, 2);
        if (id == 0) break;          /* first empty slot terminates */
        out[n].item_id  = id;
        out[n].quantity = (uint16_t)(qty ^ qty_key);
        n++;
    }
    *cnt = n;
    return PKSAVE_OK;
}

int gen3_set_item(pksave_save_t *s, pksave_pocket_t p, uint8_t slot,
                   const pksave_item_t *item)
{
    struct g3_pocket_layout layout;
    if (g3_pocket_layout(s->game, p, &layout) != PKSAVE_OK) return PKSAVE_ERR_GAME;
    if (slot >= layout.max_slots) return PKSAVE_ERR_RANGE;

    uint32_t slot_off = g3_active_slot(s->sram);
    size_t s0 = g3_find_section(s->sram, slot_off, 0);
    size_t s1 = g3_find_section(s->sram, slot_off, 1);
    if (s0 == SIZE_MAX || s1 == SIZE_MAX) return PKSAVE_ERR_CORRUPT;

    const uint8_t *sec0 = s->sram + s0;
    uint8_t *sec1 = s->sram + s1;
    uint16_t qty_key = (uint16_t)(g3_security_key(sec0, s->game) & 0xFFFF);

    uint8_t *entry = sec1 + layout.off + (size_t)slot * 4;
    uint16_t id = item->item_id;
    uint16_t qty = (uint16_t)(item->quantity ^ qty_key);
    memcpy(entry,     &id,  2);
    memcpy(entry + 2, &qty, 2);
    return PKSAVE_OK;
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
