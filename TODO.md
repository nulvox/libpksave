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

- [x] `gen1_get_pocket` — single items pocket at 0x25C9; 20 × (item_id, quantity)
- [x] `gen2_get_pocket` — items, balls, key items (TM/HM, PC box still TODO)
- [x] `gen3_get_pocket` — 5 pockets in section 1 (items, key items, balls, TM case, berries)
- [x] `gen4_get_pocket` — 5 exposed pockets (items, key items, TM/HM, berries, balls)
- [x] `gen5_get_pocket` — same pocket layout as Gen IV, BW vs BW2 offsets

## Bag / item pockets write

- [x] `gen1_set_item`
- [x] `gen2_set_item`
- [x] `gen3_set_item` — quantity XORed with low16(security key)
- [x] `gen4_set_item`
- [x] `gen5_set_item`

## Name lookup tables (`names/`)

- [x] `names/gen1_species.h` — 191 entries indexed by Gen I internal ID
- [x] `names/gen1_moves.h` — 165 moves
- [x] `names/gen1_items.h` — 0x53 items including badges
- [x] `names/gen2_species.h` — 251 entries + Egg
- [x] `names/gen2_moves.h` — 251 moves
- [x] `names/gen2_items.h` — 256 slots including TMs/HMs
- [x] `names/gen3_species.h` — 412 entries (national dex order)
- [x] `names/gen3_moves.h` — 354 moves
- [x] `names/gen3_items.h` — 377 items
- [x] `names/gen4_species.h` — 495 entries
- [x] `names/gen4_moves.h` — 467 moves
- [x] `names/gen4_items.h` — 467 items
- [x] `names/gen5_species.h` — 649 entries
- [x] `names/gen5_moves.h` — 559 moves
- [x] `names/gen5_items.h` — 639 items
- [x] Wire `pksave_species_name`, `pksave_move_name`, `pksave_item_name` in `names.c`

## Gen I/II derived fields

- [x] `is_shiny` in `gen1_get_party` / `gen1_get_box` — Gen II DV formula
- [x] `is_shiny` in `gen2_get_party` / `gen2_get_box` — same DV formula
- [x] Verify stat experience round-trip (0–65535 passthrough, not clamped to 255)

## Tests for new functions

- [x] Unity tests for all `set_party_slot` functions (mutate → flush → reopen → verify species/level)
- [x] Unity tests for all `get_box` / `set_box_slot` functions
- [x] Unity tests for all `get_pocket` / `set_item` functions
- [x] Unity test for `is_shiny` (shiny DV set vs non-shiny DV set)
- [x] Unity test for stat experience passthrough (Gen I/II)
