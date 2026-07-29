/*
 * lzx_decoder.c - LZX bitstream decoder.
 *
 * Implements the public streaming/delta decoder API declared in
 * xcompress_lzx.h. See lzx_internal.h for format notes.
 *
 * Design notes:
 *  - The decoded window history is kept as one big growable linear buffer
 *    (not a circular buffer). This is simpler and perfectly fine for a
 *    reimplementation whose primary target is buffer<->buffer/segmented use
 *    rather than unbounded low-memory streaming; matches (and the
 *    dictionary) are simply indexed by absolute position.
 *  - Each call to xlzx_decoder_decompress() is a self-contained sequence of
 *    LZX blocks (its own fresh Huffman trees, starting from all-zero
 *    "previous lengths" for the delta/RLE tree encoding) - like the
 *    original's "new group" reset - but window history and the repeated-
 *    offset (R0/R1/R2) state persist across calls so a sequence of calls
 *    compresses/decompresses as one continuous logical stream.
 */
#include "xcompress_lzx.h"
#include "lzx_internal.h"

struct xlzx_decoder {
    uint32_t window_size;
    uint32_t flags;
    uint32_t num_slots;
    uint32_t main_tree_symbols;

    uint8_t *history;
    size_t history_size;
    size_t history_capacity;

    uint32_t r0, r1, r2;
};

static xlzx_result decoder_ensure_capacity(xlzx_decoder *dec, size_t additional) {
    size_t needed = dec->history_size + additional;
    size_t new_cap;
    uint8_t *p;

    if (needed <= dec->history_capacity) return XLZX_OK;
    new_cap = dec->history_capacity ? dec->history_capacity : (size_t)65536;
    while (new_cap < needed) new_cap *= 2;

    p = (uint8_t *)realloc(dec->history, new_cap);
    if (!p) return XLZX_ERROR_OUT_OF_MEMORY;
    dec->history = p;
    dec->history_capacity = new_cap;
    return XLZX_OK;
}

static int is_power_of_two(uint32_t v) { return v != 0 && (v & (v - 1)) == 0; }

xlzx_decoder *xlzx_decoder_create(const xlzx_params *params) {
    xlzx_decoder *dec;
    uint32_t window_size = params ? params->window_size : 0;
    uint32_t flags = params ? params->flags : 0;

    if (window_size == 0) window_size = XLZX_WINDOW_SIZE_DEFAULT;
    if (!is_power_of_two(window_size) ||
        window_size < XLZX_WINDOW_SIZE_MIN ||
        window_size > XLZX_WINDOW_SIZE_MAX) {
        return NULL;
    }

    dec = (xlzx_decoder *)calloc(1, sizeof(*dec));
    if (!dec) return NULL;

    dec->window_size = window_size;
    dec->flags = flags;
    dec->num_slots = lzx_num_position_slots(window_size);
    dec->main_tree_symbols = LZX_NUM_CHARS + dec->num_slots * 8;
    dec->r0 = dec->r1 = dec->r2 = 1;
    return dec;
}

void xlzx_decoder_destroy(xlzx_decoder *dec) {
    if (!dec) return;
    free(dec->history);
    free(dec);
}

xlzx_result xlzx_decoder_reset(xlzx_decoder *dec) {
    if (!dec) return XLZX_ERROR_INVALID_PARAMETER;
    dec->history_size = 0;
    dec->r0 = dec->r1 = dec->r2 = 1;
    return XLZX_OK;
}

xlzx_result xlzx_decoder_set_dictionary(xlzx_decoder *dec, const void *data, size_t size) {
    xlzx_result rc;
    if (!dec || (size > 0 && !data)) return XLZX_ERROR_INVALID_PARAMETER;
    if (size > dec->window_size) return XLZX_ERROR_INVALID_PARAMETER;

    rc = decoder_ensure_capacity(dec, size);
    if (rc != XLZX_OK) return rc;
    memcpy(dec->history + dec->history_size, data, size);
    dec->history_size += size;
    return XLZX_OK;
}

/* Decode a pretree-coded, mod-17-delta run-length-encoded set of `count`
 * code lengths (used for the main tree literal/length halves and the
 * secondary length tree). */
static int decode_tree_lengths(lzx_bitreader *br, const uint8_t *prev_len,
                                uint8_t *out_len, int count) {
    uint8_t pretree_lengths[LZX_PRETREE_SYMBOLS];
    lzx_huffman_decoder pretree;
    int i, sym;

    for (i = 0; i < LZX_PRETREE_SYMBOLS; i++) {
        pretree_lengths[i] = (uint8_t)lzx_br_getbits(br, 4);
    }
    lzx_huffman_decoder_build(&pretree, pretree_lengths, LZX_PRETREE_SYMBOLS, LZX_PRETREE_MAX_CODE_LENGTH);

    i = 0;
    while (i < count) {
        sym = lzx_huffman_decode_symbol(br, &pretree);
        if (sym < 0) return -1;

        if (sym == LZX_PRETREE_RLE_ZERO_SHORT) {
            int run = 4 + (int)lzx_br_getbits(br, 4);
            if (run > count - i) run = count - i;
            memset(out_len + i, 0, (size_t)run);
            i += run;
        } else if (sym == LZX_PRETREE_RLE_ZERO_LONG) {
            int run = 20 + (int)lzx_br_getbits(br, 5);
            if (run > count - i) run = count - i;
            memset(out_len + i, 0, (size_t)run);
            i += run;
        } else if (sym == LZX_PRETREE_RLE_SAME) {
            int run = 4 + (int)lzx_br_getbits(br, 1);
            int sym2 = lzx_huffman_decode_symbol(br, &pretree);
            int newlen, k;
            if (sym2 < 0 || sym2 > 16) return -1;
            newlen = ((int)prev_len[i] - sym2 + 17) % 17;
            if (run > count - i) run = count - i;
            for (k = 0; k < run; k++) out_len[i + k] = (uint8_t)newlen;
            i += run;
        } else {
            int newlen = ((int)prev_len[i] - sym + 17) % 17;
            out_len[i] = (uint8_t)newlen;
            i++;
        }
    }
    return 0;
}

static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Decode one main-tree symbol (literal or length/distance match) and apply
 * it to the growing history buffer. */
static int decode_one_symbol(xlzx_decoder *dec, lzx_bitreader *br,
                              const lzx_huffman_decoder *main_tree,
                              const lzx_huffman_decoder *length_tree,
                              const lzx_huffman_decoder *aligned_tree,
                              uint32_t *bytes_out) {
    int sym = lzx_huffman_decode_symbol(br, main_tree);
    if (sym < 0) return -1;

    if (sym < LZX_NUM_CHARS) {
        if (decoder_ensure_capacity(dec, 1) != XLZX_OK) return -1;
        dec->history[dec->history_size++] = (uint8_t)sym;
        *bytes_out = 1;
        return 0;
    }
    {
        int lp = sym - LZX_NUM_CHARS;
        int length_header = lp & 7;
        int slot = lp >> 3;
        uint32_t length, distance;
        size_t src_pos, i;

        if (length_header == 7) {
            int lsym = lzx_huffman_decode_symbol(br, length_tree);
            if (lsym < 0) return -1;
            length = (uint32_t)lsym + 9;
        } else {
            length = (uint32_t)length_header + 2;
        }

        if (slot < 3) {
            distance = (slot == 0) ? dec->r0 : (slot == 1) ? dec->r1 : dec->r2;
            if (slot == 1) { dec->r1 = dec->r0; dec->r0 = distance; }
            else if (slot == 2) { dec->r2 = dec->r0; dec->r0 = distance; }
        } else {
            uint32_t extra = lzx_extra_bits[slot];
            uint32_t footer;
            if (aligned_tree != NULL && extra >= 3) {
                uint32_t raw = (extra > 3) ? lzx_br_getbits(br, (int)(extra - 3)) : 0;
                int asym = lzx_huffman_decode_symbol(br, aligned_tree);
                if (asym < 0) return -1;
                footer = (raw << 3) | (uint32_t)asym;
            } else {
                footer = lzx_br_getbits(br, (int)extra);
            }
            distance = lzx_position_base[slot] + footer;
            dec->r2 = dec->r1; dec->r1 = dec->r0; dec->r0 = distance;
        }

        if (distance == 0 || (size_t)distance > dec->history_size) return -1;
        if (decoder_ensure_capacity(dec, length) != XLZX_OK) return -1;

        src_pos = dec->history_size - distance;
        for (i = 0; i < length; i++) {
            dec->history[dec->history_size + i] = dec->history[src_pos + i];
        }
        dec->history_size += length;
        *bytes_out = length;
        return 0;
    }
}

xlzx_result xlzx_decoder_decompress(xlzx_decoder *dec,
                                     const void *src, size_t src_size,
                                     void *dst, size_t dst_capacity, size_t *out_size) {
    lzx_bitreader br;
    size_t start_history_size;
    size_t produced;
    uint8_t main_prev_len[LZX_MAIN_TREE_MAX_SYMBOLS];
    uint8_t len_prev_len[LZX_LENGTH_TREE_SYMBOLS];
    uint8_t main_len[LZX_MAIN_TREE_MAX_SYMBOLS];
    uint8_t len_len[LZX_LENGTH_TREE_SYMBOLS];
    lzx_huffman_decoder main_tree, length_tree, aligned_tree;

    if (!dec || (src_size > 0 && !src) || !out_size) return XLZX_ERROR_INVALID_PARAMETER;

    memset(main_prev_len, 0, dec->main_tree_symbols);
    memset(len_prev_len, 0, LZX_LENGTH_TREE_SYMBOLS);

    lzx_br_init(&br, (const uint8_t *)src, src_size);
    start_history_size = dec->history_size;

    while (lzx_br_bits_remaining(&br) >= 32) {
        uint32_t block_type = lzx_br_getbits(&br, 3);
        uint32_t block_size = (lzx_br_getbits(&br, 8) << 16);
        block_size |= (lzx_br_getbits(&br, 8) << 8);
        block_size |= lzx_br_getbits(&br, 8);

        if (block_size == 0) break;

        if (block_type == LZX_BLOCKTYPE_VERBATIM || block_type == LZX_BLOCKTYPE_ALIGNED) {
            uint8_t aligned_lengths[LZX_ALIGNED_TREE_SYMBOLS];
            int has_aligned = (block_type == LZX_BLOCKTYPE_ALIGNED);
            uint32_t remaining;

            if (has_aligned) {
                int i;
                for (i = 0; i < LZX_ALIGNED_TREE_SYMBOLS; i++) {
                    aligned_lengths[i] = (uint8_t)lzx_br_getbits(&br, 3);
                }
                lzx_huffman_decoder_build(&aligned_tree, aligned_lengths,
                                           LZX_ALIGNED_TREE_SYMBOLS, LZX_ALIGNED_MAX_CODE_LENGTH);
            }

            if (decode_tree_lengths(&br, main_prev_len, main_len, LZX_NUM_CHARS) != 0)
                return XLZX_ERROR_CORRUPT_DATA;
            if (decode_tree_lengths(&br, main_prev_len + LZX_NUM_CHARS, main_len + LZX_NUM_CHARS,
                                     (int)(dec->num_slots * 8)) != 0)
                return XLZX_ERROR_CORRUPT_DATA;
            if (decode_tree_lengths(&br, len_prev_len, len_len, LZX_LENGTH_TREE_SYMBOLS) != 0)
                return XLZX_ERROR_CORRUPT_DATA;

            lzx_huffman_decoder_build(&main_tree, main_len, (int)dec->main_tree_symbols, LZX_MAX_CODE_LENGTH);
            lzx_huffman_decoder_build(&length_tree, len_len, LZX_LENGTH_TREE_SYMBOLS, LZX_MAX_CODE_LENGTH);
            memcpy(main_prev_len, main_len, dec->main_tree_symbols);
            memcpy(len_prev_len, len_len, LZX_LENGTH_TREE_SYMBOLS);

            remaining = block_size;
            while (remaining > 0) {
                uint32_t produced_bytes = 0;
                if (decode_one_symbol(dec, &br, &main_tree, &length_tree,
                                       has_aligned ? &aligned_tree : NULL, &produced_bytes) != 0) {
                    return XLZX_ERROR_CORRUPT_DATA;
                }
                if (produced_bytes >= remaining) remaining = 0;
                else remaining -= produced_bytes;
            }
        } else if (block_type == LZX_BLOCKTYPE_UNCOMPRESSED) {
            const uint8_t *p = lzx_br_align_to_word(&br);
            size_t avail = (size_t)(br.end - p);
            size_t padded_size = block_size + (block_size & 1u);

            if (avail < 12 || avail - 12 < padded_size) return XLZX_ERROR_CORRUPT_DATA;

            dec->r0 = read_le32(p); p += 4;
            dec->r1 = read_le32(p); p += 4;
            dec->r2 = read_le32(p); p += 4;

            if (decoder_ensure_capacity(dec, block_size) != XLZX_OK) return XLZX_ERROR_OUT_OF_MEMORY;
            memcpy(dec->history + dec->history_size, p, block_size);
            dec->history_size += block_size;
            p += padded_size;

            lzx_br_reinit_after_uncompressed(&br, p);
        } else {
            return XLZX_ERROR_CORRUPT_DATA;
        }
    }

    produced = dec->history_size - start_history_size;
    if (produced > dst_capacity) return XLZX_ERROR_BUFFER_TOO_SMALL;
    if (produced > 0) memcpy(dst, dec->history + start_history_size, produced);
    *out_size = produced;
    return XLZX_OK;
}
