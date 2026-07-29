/*
 * lzx_tables.c - LZX position-slot tables.
 *
 * These are the standard LZX "position base" / "extra bits" tables (51
 * slots, covering window sizes up to 2 MiB), cross-checked against the
 * constants recovered from the original decompiled decoder
 * (build_global_tables / decoder_misc_init in the reverse-engineered
 * source). extra_bits[slot] is how many additional bits follow the
 * position-slot code to fully specify a match distance; position_base[slot]
 * is the smallest distance representable by that slot (footer bits of 0).
 */
#include "lzx_internal.h"

const uint8_t lzx_extra_bits[LZX_MAX_POSITION_SLOTS + 1] = {
     0,  0,  0,  0,  1,  1,  2,  2,  3,  3,
     4,  4,  5,  5,  6,  6,  7,  7,  8,  8,
     9,  9, 10, 10, 11, 11, 12, 12, 13, 13,
    14, 14, 15, 15, 16, 16, 17, 17, 17, 17,
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    17
};

const uint32_t lzx_position_base[LZX_MAX_POSITION_SLOTS + 1] = {
          0,       1,       2,       3,       4,       6,       8,      12,
         16,      24,      32,      48,      64,      96,     128,     192,
        256,     384,     512,     768,    1024,    1536,    2048,    3072,
       4096,    6144,    8192,   12288,   16384,   24576,   32768,   49152,
      65536,   98304,  131072,  196608,  262144,  393216,  524288,  655360,
     786432,  917504, 1048576, 1179648, 1310720, 1441792, 1572864, 1703936,
    1835008, 1966080, 2097152
};

uint32_t lzx_num_position_slots(uint32_t window_size) {
    uint32_t num_slots = 4;
    uint32_t cum = 4;
    uint32_t slot = 4;
    while (cum < window_size && slot < LZX_MAX_POSITION_SLOTS) {
        cum += 1u << lzx_extra_bits[slot];
        slot++;
        num_slots = slot;
    }
    return num_slots;
}
