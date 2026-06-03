# libpksave — Remaining Work

Checkboxes track completion. Check a box (`[x]`) when the implementation
is committed **and** `make test` passes for that item.

---

## Party write

- [x] `gen1_set_party_slot` — encode 44-byte party struct, recompute DVs and stat exp
- [x] `gen2_set_party_slot` — encode 48-byte struct (BE TID, BCD money region, friendship)
- [x] `gen3_set_party_slot` — re-encrypt with PID^(TID|SID<<16) PRNG, recompute section checksum
- [x] `gen4_set_party_slot` — re-encrypt with checksum PRNG, write 32-byte substructs
- [x] `gen5_set_party_slot` — same algorithm as Gen IV, different block base offset

## PC box read

- [x] `gen1_get_box` — 33-byte box mon; 12 boxes × 20 slots at 0x4000+
- [x] `gen2_get_box` — 32-byte box mon; 14 boxes × 20 slots (layout differs GS vs Crystal)
- [x] `gen3_get_box` — 80-byte box mon in storage sections 5–13; 30 slots per box
- [x] `gen4_get_box` — 136-byte box mon in storage block; 30 slots per box
- [x] `gen5_get_box` — same structure as Gen IV, different storage block offset

## PC box write

- [x] `gen1_set_box_slot`
- [x] `gen2_set_box_slot`
- [x] `gen3_set_box_slot` — re-encrypt, recompute affected section checksum
- [x] `gen4_set_box_slot` — re-encrypt with checksum PRNG
- [x] `gen5_set_box_slot`

## Bag / item pockets read

- [ ] `gen1_get_pocket` — single items pocket at 0x25C9; 20 × (item_id, quantity)
- [ ] `gen2_get_pocket` — items, key items, balls, TM/HM, PC box (offsets differ GS vs Crystal)
- [ ] `gen3_get_pocket` — 5 pockets in section 1 (items, key items, balls, TM case, berries)
- [ ] `gen4_get_pocket` — 8 pockets (items, key items, TMs, mail, medicine, berries, balls, battle)
- [ ] `gen5_get_pocket` — same pocket layout as Gen IV

## Bag / item pockets write

- [ ] `gen1_set_item`
- [ ] `gen2_set_item`
- [ ] `gen3_set_item` — recompute section 1 checksum after write
- [ ] `gen4_set_item`
- [ ] `gen5_set_item`

## Name lookup tables (`names/`)

- [ ] `names/gen1_species.h` — 151 entries, Gen I internal ID order
- [ ] `names/gen1_moves.h`
- [ ] `names/gen1_items.h`
- [ ] `names/gen2_species.h` — 251 entries
- [ ] `names/gen2_moves.h`
- [ ] `names/gen2_items.h`
- [ ] `names/gen3_species.h` — 386 entries (national dex order)
- [ ] `names/gen3_moves.h`
- [ ] `names/gen3_items.h`
- [ ] `names/gen4_species.h` — 493 entries
- [ ] `names/gen4_moves.h`
- [ ] `names/gen4_items.h`
- [ ] `names/gen5_species.h` — 649 entries
- [ ] `names/gen5_moves.h`
- [ ] `names/gen5_items.h`
- [ ] Wire `pksave_species_name`, `pksave_move_name`, `pksave_item_name` in `names.c`

## Gen I/II derived fields

- [ ] `is_shiny` in `gen1_get_party` / `gen1_get_box` — Attack/Defense/Speed/Special DVs all 10, Speed DV even
- [ ] `is_shiny` in `gen2_get_party` / `gen2_get_box` — same DV formula as Gen I
- [ ] Verify stat experience round-trip (0–65535 passthrough, not clamped to 255)

## Tests for new functions

- [ ] Unity tests for all `set_party_slot` functions (mutate → flush → reopen → verify species/level)
- [ ] Unity tests for all `get_box` / `set_box_slot` functions
- [ ] Unity tests for all `get_pocket` / `set_item` functions
- [ ] Unity test for `is_shiny` (fixture with shiny DVs and non-shiny DVs)
- [ ] Unity test for stat experience passthrough (Gen I/II)
