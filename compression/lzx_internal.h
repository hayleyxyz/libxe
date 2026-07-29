/*
 * lzx_internal.h
 *
 * Internal (non-public) definitions shared between the LZX encoder and
 * decoder translation units. Nothing in this header is part of the public
 * API - see xcompress_lzx.h for that.
 *
 * This reimplements the LZX codec used by the Xbox 360 "XCompress" library
 * (XMemCompress/XMemDecompress). The bitstream format below (block types,
 * position-slot tables, tree sizes, pretree RLE scheme, bit packing order)
 * was recovered from decompiled sources of the original DLL and matches the
 * standard documented LZX format. The internal C representations (hash
 * chains, huffman table layout, buffer management) are a fresh, simplified
 * design and do not attempt to mirror the original binary's internals.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Format constants                                                    */
/* ------------------------------------------------------------------ */

#define LZX_MIN_WINDOW_SIZE   0x8000u      /* 32 KiB */
#define LZX_MAX_WINDOW_SIZE   0x200000u    /* 2 MiB - limit of the recovered position-slot table */

#define LZX_NUM_REPEATED_OFFSETS 3

#define LZX_MIN_MATCH   2
#define LZX_MAX_MATCH   257                 /* 2 + 255 (secondary length tree covers 249 values: 9..257, or header 0..6 -> 2..8 */

#define LZX_NUM_CHARS             256       /* literal symbols in the main tree */
#define LZX_MAX_POSITION_SLOTS    50         /* enough for LZX_MAX_WINDOW_SIZE */
#define LZX_MAIN_TREE_MAX_SYMBOLS (LZX_NUM_CHARS + LZX_MAX_POSITION_SLOTS * 8) /* 656 */
#define LZX_LENGTH_TREE_SYMBOLS   249
#define LZX_ALIGNED_TREE_SYMBOLS  8
#define LZX_PRETREE_SYMBOLS       20

#define LZX_MAX_CODE_LENGTH       16        /* main / length tree codeword length limit */
#define LZX_PRETREE_MAX_CODE_LENGTH 15       /* pretree lengths are sent as raw 4-bit fields */
#define LZX_ALIGNED_MAX_CODE_LENGTH 7        /* aligned tree lengths are sent as raw 3-bit fields */

#define LZX_PRETREE_RLE_ZERO_SHORT 17
#define LZX_PRETREE_RLE_ZERO_LONG  18
#define LZX_PRETREE_RLE_SAME       19

/* block types, matches the enum recovered from the original decoder */
typedef enum lzx_block_type {
    LZX_BLOCKTYPE_INVALID      = 0,
    LZX_BLOCKTYPE_VERBATIM     = 1,
    LZX_BLOCKTYPE_ALIGNED      = 2,
    LZX_BLOCKTYPE_UNCOMPRESSED = 3
} lzx_block_type;

/* ------------------------------------------------------------------ */
/* Position slot tables (lzx_tables.c)                                 */
/* ------------------------------------------------------------------ */

extern const uint8_t  lzx_extra_bits[LZX_MAX_POSITION_SLOTS + 1];
extern const uint32_t lzx_position_base[LZX_MAX_POSITION_SLOTS + 1];

/* Number of position slots needed so that the cumulative addressable span
 * covers `window_size` bytes (mirrors the original allocate_decompression_memory
 * / comp_alloc_compress_memory loop). */
uint32_t lzx_num_position_slots(uint32_t window_size);

/* ------------------------------------------------------------------ */
/* Bit reader (MSB-first bits, packed as little-endian 16-bit words)   */
/* ------------------------------------------------------------------ */

typedef struct lzx_bitreader {
    const uint8_t *cur;
    const uint8_t *end;
    uint32_t bitbuf;   /* left-justified; the top `bitcount` bits are valid */
    int bitcount;
} lzx_bitreader;

static inline void lzx_br_init(lzx_bitreader *br, const uint8_t *data, size_t size) {
    br->cur = data;
    br->end = data + size;
    br->bitbuf = 0;
    br->bitcount = 0;
}

/* Ensure at least 17 valid bits (the largest single field we ever read). */
static inline void lzx_br_fill(lzx_bitreader *br) {
    while (br->bitcount <= 16) {
        uint32_t word;
        if (br->cur + 2 <= br->end) {
            word = (uint32_t)br->cur[0] | ((uint32_t)br->cur[1] << 8);
            br->cur += 2;
        } else if (br->cur + 1 <= br->end) {
            word = (uint32_t)br->cur[0];
            br->cur += 1;
        } else {
            word = 0; /* padding beyond end of buffer; treated as zero bits */
        }
        br->bitbuf |= word << (16 - br->bitcount);
        br->bitcount += 16;
    }
}

static inline uint32_t lzx_br_getbits(lzx_bitreader *br, int n) {
    uint32_t result;
    if (n == 0) return 0;
    lzx_br_fill(br);
    result = br->bitbuf >> (32 - n);
    br->bitbuf <<= n;
    br->bitcount -= n;
    return result;
}

/* Number of unconsumed bits still available (input left + buffered bits). */
static inline size_t lzx_br_bits_remaining(const lzx_bitreader *br) {
    return (size_t)(br->end - br->cur) * 8 + (size_t)(br->bitcount > 0 ? br->bitcount : 0);
}

/* Re-synchronise to a 16-bit word boundary and return the current byte
 * pointer (used after an uncompressed block). Any bits still marked as
 * "buffered" but not actually consumed are pushed back. */
static inline const uint8_t *lzx_br_align_to_word(lzx_bitreader *br) {
    int buffered_whole_words = br->bitcount / 16;
    br->cur -= (size_t)buffered_whole_words * 2;
    br->bitbuf = 0;
    br->bitcount = 0;
    return br->cur;
}

static inline void lzx_br_reinit_after_uncompressed(lzx_bitreader *br, const uint8_t *new_pos) {
    br->cur = new_pos;
    br->bitbuf = 0;
    br->bitcount = 0;
}

/* ------------------------------------------------------------------ */
/* Bit writer (mirrors the reader exactly)                             */
/* ------------------------------------------------------------------ */

typedef struct lzx_bitwriter {
    uint8_t *cur;
    uint8_t *end;
    uint32_t bitbuf;
    int bitcount;     /* number of bits currently buffered, 0..15 kept, flush at >=16 */
    int overflow;     /* set if the destination buffer was too small */
} lzx_bitwriter;

static inline void lzx_bw_init(lzx_bitwriter *bw, uint8_t *dst, size_t capacity) {
    bw->cur = dst;
    bw->end = dst + capacity;
    bw->bitbuf = 0;
    bw->bitcount = 0;
    bw->overflow = 0;
}

static inline void lzx_bw_putbits(lzx_bitwriter *bw, int n, uint32_t value) {
    if (n == 0) return;
    bw->bitbuf |= (value & ((n < 32) ? ((1u << n) - 1u) : 0xFFFFFFFFu)) << (32 - bw->bitcount - n);
    bw->bitcount += n;
    while (bw->bitcount >= 16) {
        if (bw->cur + 2 > bw->end) {
            bw->overflow = 1;
        } else {
            bw->cur[0] = (uint8_t)(bw->bitbuf >> 16);
            bw->cur[1] = (uint8_t)(bw->bitbuf >> 24);
        }
        bw->cur += 2;
        bw->bitbuf <<= 16;
        bw->bitcount -= 16;
    }
}

/* Pad up to the next 16-bit boundary with zero bits. */
static inline void lzx_bw_align(lzx_bitwriter *bw) {
    if (bw->bitcount > 0) {
        lzx_bw_putbits(bw, 16 - bw->bitcount, 0);
    }
}

static inline size_t lzx_bw_bytes_written(const lzx_bitwriter *bw, const uint8_t *dst_start) {
    return (size_t)(bw->cur - dst_start);
}

/* ------------------------------------------------------------------ */
/* Canonical Huffman helpers                                           */
/*                                                                      */
/* Only the *external* bit<->symbol mapping needs to match the LZX      */
/* spec (canonical Huffman assignment is a standard algorithm, not an   */
/* implementation detail), so both sides here use a simple, robust      */
/* table-free "peel one bit at a time" scheme (same idea as Mark        */
/* Adler's public-domain puff.c reference inflate decoder).             */
/* ------------------------------------------------------------------ */

typedef struct lzx_huffman_decoder {
    int max_length;
    int num_symbols;
    uint16_t count[LZX_MAX_CODE_LENGTH + 2];        /* codes of each length */
    uint16_t first_code[LZX_MAX_CODE_LENGTH + 2];
    uint16_t first_index[LZX_MAX_CODE_LENGTH + 2];
    uint16_t sorted_symbols[LZX_MAIN_TREE_MAX_SYMBOLS];
} lzx_huffman_decoder;

/* Build a decode structure from an array of code lengths (0 = unused). */
static inline void lzx_huffman_decoder_build(lzx_huffman_decoder *h,
                                              const uint8_t *lengths,
                                              int num_symbols, int max_length) {
    int i, len;
    uint16_t offsets[LZX_MAX_CODE_LENGTH + 2];
    uint16_t next[LZX_MAX_CODE_LENGTH + 2];
    uint32_t code;

    h->max_length = max_length;
    h->num_symbols = num_symbols;
    memset(h->count, 0, sizeof(h->count));
    for (i = 0; i < num_symbols; i++) {
        h->count[lengths[i]]++;
    }
    h->count[0] = 0; /* unused symbols never match a code */

    offsets[0] = 0;
    offsets[1] = 0;
    for (len = 1; len <= max_length; len++) {
        offsets[len + 1] = (uint16_t)(offsets[len] + h->count[len]);
    }
    memcpy(next, offsets, sizeof(offsets));
    for (i = 0; i < num_symbols; i++) {
        len = lengths[i];
        if (len == 0) continue;
        h->sorted_symbols[next[len]++] = (uint16_t)i;
    }

    code = 0;
    for (len = 1; len <= max_length; len++) {
        h->first_index[len] = offsets[len];
        h->first_code[len] = (uint16_t)code;
        code = (code + h->count[len]) << 1;
    }
}

/* Returns the decoded symbol, or -1 on a corrupt/invalid code. */
static inline int lzx_huffman_decode_symbol(lzx_bitreader *br, const lzx_huffman_decoder *h) {
    uint32_t code = 0, first = 0;
    int len;
    for (len = 1; len <= h->max_length; len++) {
        uint32_t idx;
        code |= lzx_br_getbits(br, 1);
        idx = code - first;
        if ((int)idx < h->count[len]) {
            return h->sorted_symbols[h->first_index[len] + idx];
        }
        first = (first + h->count[len]) << 1;
        code <<= 1;
    }
    return -1;
}

/* Assign canonical (MSB-first) codes for encoding, given lengths[]. */
static inline void lzx_huffman_build_encode_table(const uint8_t *lengths, uint16_t *codes,
                                                   int num_symbols, int max_length) {
    uint16_t count[LZX_MAX_CODE_LENGTH + 2];
    uint16_t next[LZX_MAX_CODE_LENGTH + 2];
    int i, len;
    uint32_t code;

    memset(count, 0, sizeof(count));
    for (i = 0; i < num_symbols; i++) count[lengths[i]]++;
    count[0] = 0;

    next[0] = 0;
    code = 0;
    for (len = 1; len <= max_length; len++) {
        next[len] = (uint16_t)code;
        code = (code + count[len]) << 1;
    }
    for (i = 0; i < num_symbols; i++) {
        len = lengths[i];
        if (len == 0) { codes[i] = 0; continue; }
        codes[i] = next[len]++;
    }
}

/* Length-limited Huffman code length assignment from symbol frequencies.
 * Simple O(n^2) tree build (n <= LZX_MAIN_TREE_MAX_SYMBOLS = 656, so this is
 * cheap) plus a "rescale and retry" loop to guarantee max_length is honoured
 * even for pathological/skewed frequency distributions. */
void lzx_build_huffman_lengths(const uint32_t *freq, uint8_t *lengths,
                                int num_symbols, int max_length);

/* ------------------------------------------------------------------ */
/* Misc shared helpers                                                  */
/* ------------------------------------------------------------------ */

static inline size_t lzx_match_length(const uint8_t *a, const uint8_t *b, size_t max_len) {
    size_t n = 0;
    while (n < max_len && a[n] == b[n]) n++;
    return n;
}
