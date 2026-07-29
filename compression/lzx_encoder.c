/*
 * lzx_encoder.c - LZX bitstream encoder.
 *
 * Implements the public streaming/delta encoder API declared in
 * xcompress_lzx.h. See lzx_internal.h for format notes and lzx_decoder.c
 * for the matching read side.
 *
 * This encoder deliberately only ever emits VERBATIM or UNCOMPRESSED blocks
 * (never ALIGNED). Aligned-offset blocks are a pure compression-ratio
 * optimisation on top of an already-valid LZX stream; skipping them keeps
 * the encoder much simpler while remaining fully spec-compliant output that
 * any conformant LZX decoder (including lzx_decoder.c, which supports all
 * three block types) can read.
 *
 * Match finding is a simple hash-chain (+ 1-step lazy evaluation) scheme
 * rather than the original's binary-tree optimal parse - simpler, and the
 * task only requires a correct, reasonably efficient codec, not a bit-exact
 * reproduction of the original compressor's output.
 */
#include "xcompress_lzx.h"
#include "lzx_internal.h"

#define LZX_ENCODE_BLOCK_SIZE 32768u
#define LZX_HASH_BITS 15
#define LZX_HASH_SIZE (1u << LZX_HASH_BITS)
#define LZX_MAX_CHAIN_STEPS 32

typedef struct lzx_item {
    uint8_t is_match;
    uint8_t literal;
    uint16_t main_symbol;
    uint16_t length_symbol;   /* valid when length_header == 7 */
    uint8_t length_header;    /* 0..7, informational */
    uint8_t footer_bits;
    uint32_t footer_value;
} lzx_item;

struct xlzx_encoder {
    uint32_t window_size;
    uint32_t flags;
    uint32_t num_slots;
    uint32_t main_tree_symbols;

    uint8_t *history;
    size_t history_size;
    size_t history_capacity;

    int32_t *hash_head;
    int32_t *prev_chain;   /* sized to history_capacity */

    uint32_t r0, r1, r2;

    lzx_item *items;        /* LZX_ENCODE_BLOCK_SIZE entries */
    uint32_t *main_freq;     /* main_tree_symbols entries */
    uint32_t *len_freq;      /* LZX_LENGTH_TREE_SYMBOLS entries */
    uint8_t *main_len;
    uint8_t *len_len;
    uint16_t *main_codes;
    uint16_t *len_codes;
};

static int is_power_of_two(uint32_t v) { return v != 0 && (v & (v - 1)) == 0; }

/* ------------------------------------------------------------------ */
/* Length-limited Huffman code length assignment                       */
/* ------------------------------------------------------------------ */

void lzx_build_huffman_lengths(const uint32_t *freq, uint8_t *lengths,
                                int num_symbols, int max_length) {
    uint32_t node_freq[2 * LZX_MAIN_TREE_MAX_SYMBOLS];
    int parent[2 * LZX_MAIN_TREE_MAX_SYMBOLS];
    int leaf_node[LZX_MAIN_TREE_MAX_SYMBOLS];
    uint32_t work_freq[LZX_MAIN_TREE_MAX_SYMBOLS];
    int active[LZX_MAIN_TREE_MAX_SYMBOLS];
    int i;

    memset(lengths, 0, (size_t)num_symbols);
    memcpy(work_freq, freq, sizeof(uint32_t) * (size_t)num_symbols);

    for (;;) {
        int num_active = 0;
        int num_nodes = 0;
        int nonzero_count = 0;
        int single_symbol = -1;
        int max_depth = 0;
        int overflow = 0;

        for (i = 0; i < num_symbols; i++) {
            if (work_freq[i] > 0) {
                node_freq[num_nodes] = work_freq[i];
                parent[num_nodes] = -1;
                leaf_node[i] = num_nodes;
                active[num_active++] = num_nodes;
                num_nodes++;
                nonzero_count++;
                single_symbol = i;
            } else {
                leaf_node[i] = -1;
            }
        }

        if (nonzero_count == 0) return;
        if (nonzero_count == 1) { lengths[single_symbol] = 1; return; }

        while (num_active > 1) {
            int a = 0, b = 1, lo, hi, node_lo, node_hi, newnode;
            if (node_freq[active[b]] < node_freq[active[a]]) { int t = a; a = b; b = t; }
            for (i = 2; i < num_active; i++) {
                if (node_freq[active[i]] < node_freq[active[a]]) { b = a; a = i; }
                else if (node_freq[active[i]] < node_freq[active[b]]) { b = i; }
            }
            lo = (a < b) ? a : b;
            hi = (a < b) ? b : a;
            node_lo = active[lo];
            node_hi = active[hi];

            newnode = num_nodes++;
            node_freq[newnode] = node_freq[node_lo] + node_freq[node_hi];
            parent[newnode] = -1;
            parent[node_lo] = newnode;
            parent[node_hi] = newnode;

            active[hi] = active[num_active - 1]; num_active--;
            active[lo] = active[num_active - 1]; num_active--;
            active[num_active++] = newnode;
        }

        for (i = 0; i < num_symbols; i++) {
            if (leaf_node[i] < 0) { lengths[i] = 0; continue; }
            {
                int depth = 0, n = leaf_node[i];
                while (parent[n] >= 0) { n = parent[n]; depth++; }
                if (depth > max_length) overflow = 1;
                if (depth > max_depth) max_depth = depth;
                if (depth > 255) depth = 255;
                lengths[i] = (uint8_t)depth;
            }
        }
        if (!overflow) return;

        for (i = 0; i < num_symbols; i++) {
            if (work_freq[i] > 1) work_freq[i] = (work_freq[i] + 1) / 2;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                            */
/* ------------------------------------------------------------------ */

xlzx_encoder *xlzx_encoder_create(const xlzx_params *params) {
    xlzx_encoder *enc;
    uint32_t window_size = params ? params->window_size : 0;
    uint32_t flags = params ? params->flags : 0;

    if (window_size == 0) window_size = XLZX_WINDOW_SIZE_DEFAULT;
    if (!is_power_of_two(window_size) ||
        window_size < XLZX_WINDOW_SIZE_MIN ||
        window_size > XLZX_WINDOW_SIZE_MAX) {
        return NULL;
    }

    enc = (xlzx_encoder *)calloc(1, sizeof(*enc));
    if (!enc) return NULL;

    enc->window_size = window_size;
    enc->flags = flags;
    enc->num_slots = lzx_num_position_slots(window_size);
    enc->main_tree_symbols = LZX_NUM_CHARS + enc->num_slots * 8;
    enc->r0 = enc->r1 = enc->r2 = 1;

    enc->hash_head = (int32_t *)malloc(sizeof(int32_t) * LZX_HASH_SIZE);
    enc->items = (lzx_item *)malloc(sizeof(lzx_item) * LZX_ENCODE_BLOCK_SIZE);
    enc->main_freq = (uint32_t *)malloc(sizeof(uint32_t) * enc->main_tree_symbols);
    enc->len_freq = (uint32_t *)malloc(sizeof(uint32_t) * LZX_LENGTH_TREE_SYMBOLS);
    enc->main_len = (uint8_t *)malloc(enc->main_tree_symbols);
    enc->len_len = (uint8_t *)malloc(LZX_LENGTH_TREE_SYMBOLS);
    enc->main_codes = (uint16_t *)malloc(sizeof(uint16_t) * enc->main_tree_symbols);
    enc->len_codes = (uint16_t *)malloc(sizeof(uint16_t) * LZX_LENGTH_TREE_SYMBOLS);

    if (!enc->hash_head || !enc->items || !enc->main_freq || !enc->len_freq ||
        !enc->main_len || !enc->len_len || !enc->main_codes || !enc->len_codes) {
        xlzx_encoder_destroy(enc);
        return NULL;
    }
    memset(enc->hash_head, 0xFF, sizeof(int32_t) * LZX_HASH_SIZE); /* -1 = empty */
    return enc;
}

void xlzx_encoder_destroy(xlzx_encoder *enc) {
    if (!enc) return;
    free(enc->history);
    free(enc->hash_head);
    free(enc->prev_chain);
    free(enc->items);
    free(enc->main_freq);
    free(enc->len_freq);
    free(enc->main_len);
    free(enc->len_len);
    free(enc->main_codes);
    free(enc->len_codes);
    free(enc);
}

xlzx_result xlzx_encoder_reset(xlzx_encoder *enc) {
    if (!enc) return XLZX_ERROR_INVALID_PARAMETER;
    enc->history_size = 0;
    enc->r0 = enc->r1 = enc->r2 = 1;
    memset(enc->hash_head, 0xFF, sizeof(int32_t) * LZX_HASH_SIZE);
    return XLZX_OK;
}

/* ------------------------------------------------------------------ */
/* History buffer + match finder                                       */
/* ------------------------------------------------------------------ */

static xlzx_result encoder_ensure_capacity(xlzx_encoder *enc, size_t additional) {
    size_t needed = enc->history_size + additional;
    size_t new_cap;
    uint8_t *h;
    int32_t *pc;

    if (needed <= enc->history_capacity) return XLZX_OK;
    new_cap = enc->history_capacity ? enc->history_capacity : (size_t)65536;
    while (new_cap < needed) new_cap *= 2;

    h = (uint8_t *)realloc(enc->history, new_cap);
    if (!h) return XLZX_ERROR_OUT_OF_MEMORY;
    enc->history = h;

    pc = (int32_t *)realloc(enc->prev_chain, new_cap * sizeof(int32_t));
    if (!pc) return XLZX_ERROR_OUT_OF_MEMORY;
    enc->prev_chain = pc;

    enc->history_capacity = new_cap;
    return XLZX_OK;
}

static uint32_t hash3(const uint8_t *p) {
    uint32_t v = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
    return (v * 2654435761u) >> (32 - LZX_HASH_BITS);
}

static void insert_position(xlzx_encoder *enc, size_t pos) {
    uint32_t h;
    if (pos + 3 > enc->history_size) return;
    h = hash3(enc->history + pos);
    enc->prev_chain[pos] = enc->hash_head[h];
    enc->hash_head[h] = (int32_t)pos;
}

static void insert_range(xlzx_encoder *enc, size_t start, size_t end) {
    size_t p;
    for (p = start; p < end; p++) insert_position(enc, p);
}

static uint32_t check_repeat(xlzx_encoder *enc, size_t pos, uint32_t offset, size_t max_len) {
    if (offset == 0 || (size_t)offset > pos) return 0;
    return (uint32_t)lzx_match_length(enc->history + pos - offset, enc->history + pos, max_len);
}

/* Hash-chain search for a new (non-repeat) match. Distances 1 and 2 are
 * never returned since they have no valid "new distance" encoding in LZX
 * (slots 0..2 are reserved for repeated offsets). */
static void find_new_match(xlzx_encoder *enc, size_t pos, size_t max_len,
                            uint32_t *out_len, uint32_t *out_dist) {
    size_t window_start = (pos >= enc->window_size) ? pos - enc->window_size : 0;
    int32_t cand;
    int steps = 0;
    uint32_t best_len = 0, best_dist = 0;

    *out_len = 0; *out_dist = 0;
    if (pos + 3 > enc->history_size) return;

    cand = enc->hash_head[hash3(enc->history + pos)];
    while (cand >= 0 && (size_t)cand >= window_start && steps < LZX_MAX_CHAIN_STEPS) {
        size_t dist = pos - (size_t)cand;
        if (dist >= 3) {
            uint32_t len = (uint32_t)lzx_match_length(enc->history + cand, enc->history + pos, max_len);
            if (len > best_len) {
                best_len = len;
                best_dist = (uint32_t)dist;
                if (len >= max_len) break;
            }
        }
        cand = enc->prev_chain[cand];
        steps++;
    }
    *out_len = best_len;
    *out_dist = best_dist;
}

typedef struct { uint32_t length; uint32_t distance; int is_repeat; int repeat_slot; } best_match;

static void find_best_match(xlzx_encoder *enc, size_t pos, size_t max_len, best_match *out) {
    uint32_t r0len, r1len, r2len, new_len, new_dist;
    uint32_t best_repeat_len = 0;
    int best_repeat_slot = 0;

    out->length = 0; out->distance = 0; out->is_repeat = 0; out->repeat_slot = 0;
    if (max_len < LZX_MIN_MATCH) return;

    r0len = check_repeat(enc, pos, enc->r0, max_len);
    r1len = check_repeat(enc, pos, enc->r1, max_len);
    r2len = check_repeat(enc, pos, enc->r2, max_len);
    if (r0len >= 2) { best_repeat_len = r0len; best_repeat_slot = 0; }
    if (r1len >= 2 && r1len > best_repeat_len) { best_repeat_len = r1len; best_repeat_slot = 1; }
    if (r2len >= 2 && r2len > best_repeat_len) { best_repeat_len = r2len; best_repeat_slot = 2; }

    find_new_match(enc, pos, max_len, &new_len, &new_dist);

    if (best_repeat_len >= 2 && (new_len < 3 || best_repeat_len + 1 >= new_len)) {
        out->length = best_repeat_len;
        out->is_repeat = 1;
        out->repeat_slot = best_repeat_slot;
        out->distance = (best_repeat_slot == 0) ? enc->r0 : (best_repeat_slot == 1) ? enc->r1 : enc->r2;
    } else if (new_len >= 3) {
        out->length = new_len;
        out->distance = new_dist;
        out->is_repeat = 0;
    }
}

static int find_slot_for_distance(uint32_t distance, uint32_t num_slots) {
    int slot = 0;
    int limit = (int)num_slots - 1;
    while (slot < limit && lzx_position_base[slot + 1] <= distance) slot++;
    return slot;
}

/* ------------------------------------------------------------------ */
/* Pretree (tree length) transmission                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t symbol;
    uint8_t extra_bits;
    uint32_t extra_value;
    uint8_t has_second;
    uint8_t second_symbol;
} tree_op;

static uint8_t pretree_delta_symbol(uint8_t prev_len, uint8_t new_len) {
    return (uint8_t)(((int)prev_len - (int)new_len + 17) % 17);
}

static int plan_tree_ops(const uint8_t *prev_len, const uint8_t *new_len, int count, tree_op *ops) {
    int i = 0, nops = 0;
    while (i < count) {
        int run = 1;
        while (i + run < count && new_len[i + run] == new_len[i]) run++;

        if (new_len[i] == 0 && run >= 4) {
            int take = run > 51 ? 51 : run;
            if (take < 20) {
                ops[nops].symbol = LZX_PRETREE_RLE_ZERO_SHORT;
                ops[nops].extra_bits = 4;
                ops[nops].extra_value = (uint32_t)(take - 4);
            } else {
                ops[nops].symbol = LZX_PRETREE_RLE_ZERO_LONG;
                ops[nops].extra_bits = 5;
                ops[nops].extra_value = (uint32_t)(take - 20);
            }
            ops[nops].has_second = 0;
            nops++;
            i += take;
        } else if (new_len[i] != 0 && run >= 4) {
            int take = run > 5 ? 5 : run;
            ops[nops].symbol = LZX_PRETREE_RLE_SAME;
            ops[nops].extra_bits = 1;
            ops[nops].extra_value = (uint32_t)(take - 4);
            ops[nops].has_second = 1;
            ops[nops].second_symbol = pretree_delta_symbol(prev_len[i], new_len[i]);
            nops++;
            i += take;
        } else {
            ops[nops].symbol = pretree_delta_symbol(prev_len[i], new_len[i]);
            ops[nops].extra_bits = 0;
            ops[nops].has_second = 0;
            nops++;
            i += 1;
        }
    }
    return nops;
}

static void write_tree_lengths(lzx_bitwriter *bw, const uint8_t *prev_len,
                                const uint8_t *new_len, int count) {
    tree_op ops[LZX_MAIN_TREE_MAX_SYMBOLS];
    uint32_t freq[LZX_PRETREE_SYMBOLS];
    uint8_t pretree_lengths[LZX_PRETREE_SYMBOLS];
    uint16_t pretree_codes[LZX_PRETREE_SYMBOLS];
    int nops, i;

    nops = plan_tree_ops(prev_len, new_len, count, ops);

    memset(freq, 0, sizeof(freq));
    for (i = 0; i < nops; i++) {
        freq[ops[i].symbol]++;
        if (ops[i].has_second) freq[ops[i].second_symbol]++;
    }
    lzx_build_huffman_lengths(freq, pretree_lengths, LZX_PRETREE_SYMBOLS, LZX_PRETREE_MAX_CODE_LENGTH);
    lzx_huffman_build_encode_table(pretree_lengths, pretree_codes, LZX_PRETREE_SYMBOLS, LZX_PRETREE_MAX_CODE_LENGTH);

    for (i = 0; i < LZX_PRETREE_SYMBOLS; i++) {
        lzx_bw_putbits(bw, 4, pretree_lengths[i]);
    }
    for (i = 0; i < nops; i++) {
        lzx_bw_putbits(bw, pretree_lengths[ops[i].symbol], pretree_codes[ops[i].symbol]);
        if (ops[i].extra_bits) lzx_bw_putbits(bw, ops[i].extra_bits, ops[i].extra_value);
        if (ops[i].has_second) {
            lzx_bw_putbits(bw, pretree_lengths[ops[i].second_symbol], pretree_codes[ops[i].second_symbol]);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Block writers                                                        */
/* ------------------------------------------------------------------ */

static void bw_write_raw_bytes(lzx_bitwriter *bw, const uint8_t *data, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (bw->cur + 1 > bw->end) { bw->overflow = 1; return; }
        *bw->cur++ = data[i];
    }
}

static void bw_write_raw_u32le(lzx_bitwriter *bw, uint32_t v) {
    uint8_t b[4];
    b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24);
    bw_write_raw_bytes(bw, b, 4);
}

static void write_block_header(lzx_bitwriter *bw, lzx_block_type type, uint32_t block_size) {
    lzx_bw_putbits(bw, 3, (uint32_t)type);
    lzx_bw_putbits(bw, 8, (block_size >> 16) & 0xFF);
    lzx_bw_putbits(bw, 8, (block_size >> 8) & 0xFF);
    lzx_bw_putbits(bw, 8, block_size & 0xFF);
}

static void write_block_uncompressed(lzx_bitwriter *bw, xlzx_encoder *enc,
                                      size_t block_start, uint32_t block_size) {
    write_block_header(bw, LZX_BLOCKTYPE_UNCOMPRESSED, block_size);
    lzx_bw_align(bw);
    bw_write_raw_u32le(bw, enc->r0);
    bw_write_raw_u32le(bw, enc->r1);
    bw_write_raw_u32le(bw, enc->r2);
    bw_write_raw_bytes(bw, enc->history + block_start, block_size);
    if (block_size & 1u) {
        uint8_t pad = 0;
        bw_write_raw_bytes(bw, &pad, 1);
    }
}

/* Returns estimated bit cost of verbatim-encoding `nitems`, and fills
 * enc->main_len/len_len/main_codes/len_codes ready for writing. */
static uint64_t prepare_verbatim_tables(xlzx_encoder *enc, const lzx_item *items, size_t nitems) {
    uint64_t bits = 0;
    size_t i;

    memset(enc->main_freq, 0, sizeof(uint32_t) * enc->main_tree_symbols);
    memset(enc->len_freq, 0, sizeof(uint32_t) * LZX_LENGTH_TREE_SYMBOLS);

    for (i = 0; i < nitems; i++) {
        if (!items[i].is_match) enc->main_freq[items[i].literal]++;
        else {
            enc->main_freq[items[i].main_symbol]++;
            if (items[i].length_header == 7) enc->len_freq[items[i].length_symbol]++;
        }
    }

    lzx_build_huffman_lengths(enc->main_freq, enc->main_len, (int)enc->main_tree_symbols, LZX_MAX_CODE_LENGTH);
    lzx_build_huffman_lengths(enc->len_freq, enc->len_len, LZX_LENGTH_TREE_SYMBOLS, LZX_MAX_CODE_LENGTH);
    lzx_huffman_build_encode_table(enc->main_len, enc->main_codes, (int)enc->main_tree_symbols, LZX_MAX_CODE_LENGTH);
    lzx_huffman_build_encode_table(enc->len_len, enc->len_codes, LZX_LENGTH_TREE_SYMBOLS, LZX_MAX_CODE_LENGTH);

    for (i = 0; i < nitems; i++) {
        if (!items[i].is_match) bits += enc->main_len[items[i].literal];
        else {
            bits += enc->main_len[items[i].main_symbol];
            if (items[i].length_header == 7) bits += enc->len_len[items[i].length_symbol];
            bits += items[i].footer_bits;
        }
    }
    return bits;
}

static void write_block_verbatim(lzx_bitwriter *bw, xlzx_encoder *enc,
                                  const lzx_item *items, size_t nitems, uint32_t block_size,
                                  uint8_t *main_prev_len, uint8_t *len_prev_len) {
    size_t i;
    write_block_header(bw, LZX_BLOCKTYPE_VERBATIM, block_size);

    write_tree_lengths(bw, main_prev_len, enc->main_len, LZX_NUM_CHARS);
    write_tree_lengths(bw, main_prev_len + LZX_NUM_CHARS, enc->main_len + LZX_NUM_CHARS,
                        (int)(enc->num_slots * 8));
    write_tree_lengths(bw, len_prev_len, enc->len_len, LZX_LENGTH_TREE_SYMBOLS);

    memcpy(main_prev_len, enc->main_len, enc->main_tree_symbols);
    memcpy(len_prev_len, enc->len_len, LZX_LENGTH_TREE_SYMBOLS);

    for (i = 0; i < nitems; i++) {
        if (!items[i].is_match) {
            lzx_bw_putbits(bw, enc->main_len[items[i].literal], enc->main_codes[items[i].literal]);
        } else {
            lzx_bw_putbits(bw, enc->main_len[items[i].main_symbol], enc->main_codes[items[i].main_symbol]);
            if (items[i].length_header == 7) {
                lzx_bw_putbits(bw, enc->len_len[items[i].length_symbol], enc->len_codes[items[i].length_symbol]);
            }
            if (items[i].footer_bits) {
                lzx_bw_putbits(bw, items[i].footer_bits, items[i].footer_value);
            }
        }
    }
}

/* Builds the 8-symbol aligned-offset tree from the low 3 bits of every
 * match's position footer (only matches with footer_bits >= 3 use it), and
 * returns the *change* in total bit cost relative to prepare_verbatim_tables()'s
 * result if an ALIGNED block were used instead of VERBATIM for the same
 * item list (negative = aligned block would be smaller). */
static int64_t prepare_aligned_tree(const lzx_item *items, size_t nitems,
 uint8_t *aligned_len, uint16_t *aligned_codes) {
    uint32_t freq[LZX_ALIGNED_TREE_SYMBOLS];
    int64_t delta = 0;
    size_t i;

    memset(freq, 0, sizeof(freq));
    for (i = 0; i < nitems; i++) {
        if (items[i].is_match && items[i].footer_bits >= 3) {
            freq[items[i].footer_value & 7]++;
        }
    }
    lzx_build_huffman_lengths(freq, aligned_len, LZX_ALIGNED_TREE_SYMBOLS, LZX_ALIGNED_MAX_CODE_LENGTH);
    lzx_huffman_build_encode_table(aligned_len, aligned_codes, LZX_ALIGNED_TREE_SYMBOLS, LZX_ALIGNED_MAX_CODE_LENGTH);

    for (i = 0; i < nitems; i++) {
        if (items[i].is_match && items[i].footer_bits >= 3) {
            int low3 = items[i].footer_value & 7;
            int new_cost = (items[i].footer_bits - 3) + aligned_len[low3];
            delta += new_cost - items[i].footer_bits;
        }
    }
    delta += LZX_ALIGNED_TREE_SYMBOLS * 3; /* transmitting the 8x3-bit aligned lengths */
    return delta;
}

static void write_block_aligned(lzx_bitwriter *bw, xlzx_encoder *enc,
                                 const lzx_item *items, size_t nitems, uint32_t block_size,
                                 const uint8_t *aligned_len, const uint16_t *aligned_codes,
                                 uint8_t *main_prev_len, uint8_t *len_prev_len) {
    size_t i;
    write_block_header(bw, LZX_BLOCKTYPE_ALIGNED, block_size);

    for (i = 0; i < LZX_ALIGNED_TREE_SYMBOLS; i++) lzx_bw_putbits(bw, 3, aligned_len[i]);

    write_tree_lengths(bw, main_prev_len, enc->main_len, LZX_NUM_CHARS);
    write_tree_lengths(bw, main_prev_len + LZX_NUM_CHARS, enc->main_len + LZX_NUM_CHARS,
                        (int)(enc->num_slots * 8));
    write_tree_lengths(bw, len_prev_len, enc->len_len, LZX_LENGTH_TREE_SYMBOLS);

    memcpy(main_prev_len, enc->main_len, enc->main_tree_symbols);
    memcpy(len_prev_len, enc->len_len, LZX_LENGTH_TREE_SYMBOLS);

    for (i = 0; i < nitems; i++) {
        if (!items[i].is_match) {
            lzx_bw_putbits(bw, enc->main_len[items[i].literal], enc->main_codes[items[i].literal]);
            continue;
        }
        lzx_bw_putbits(bw, enc->main_len[items[i].main_symbol], enc->main_codes[items[i].main_symbol]);
        if (items[i].length_header == 7) {
            lzx_bw_putbits(bw, enc->len_len[items[i].length_symbol], enc->len_codes[items[i].length_symbol]);
        }
        if (items[i].footer_bits >= 3) {
            int low3 = items[i].footer_value & 7;
            uint32_t high = items[i].footer_value >> 3;
            if (items[i].footer_bits > 3) lzx_bw_putbits(bw, items[i].footer_bits - 3, high);
            lzx_bw_putbits(bw, aligned_len[low3], aligned_codes[low3]);
        } else if (items[i].footer_bits) {
            lzx_bw_putbits(bw, items[i].footer_bits, items[i].footer_value);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Dictionary                                                           */
/* ------------------------------------------------------------------ */

xlzx_result xlzx_encoder_set_dictionary(xlzx_encoder *enc, const void *data, size_t size) {
    xlzx_result rc;
    size_t old_size;
    if (!enc || (size > 0 && !data)) return XLZX_ERROR_INVALID_PARAMETER;
    if (size > enc->window_size) return XLZX_ERROR_INVALID_PARAMETER;

    rc = encoder_ensure_capacity(enc, size);
    if (rc != XLZX_OK) return rc;

    old_size = enc->history_size;
    memcpy(enc->history + old_size, data, size);
    enc->history_size += size;
    insert_range(enc, old_size, enc->history_size);
    return XLZX_OK;
}

/* ------------------------------------------------------------------ */
/* Top level compress                                                   */
/* ------------------------------------------------------------------ */

xlzx_result xlzx_encoder_compress(xlzx_encoder *enc,
                                   const void *src, size_t src_size,
                                   void *dst, size_t dst_capacity, size_t *out_size) {
    lzx_bitwriter bw;
    uint8_t main_prev_len[LZX_MAIN_TREE_MAX_SYMBOLS];
    uint8_t len_prev_len[LZX_LENGTH_TREE_SYMBOLS];
    size_t pos, end;
    xlzx_result rc;

    if (!enc || (src_size > 0 && !src) || !out_size) return XLZX_ERROR_INVALID_PARAMETER;

    rc = encoder_ensure_capacity(enc, src_size);
    if (rc != XLZX_OK) return rc;
    memcpy(enc->history + enc->history_size, src, src_size);
    pos = enc->history_size;
    enc->history_size += src_size;
    end = enc->history_size;

    memset(main_prev_len, 0, enc->main_tree_symbols);
    memset(len_prev_len, 0, LZX_LENGTH_TREE_SYMBOLS);

    lzx_bw_init(&bw, (uint8_t *)dst, dst_capacity);

    while (pos < end) {
        size_t block_start = pos;
        size_t block_target_end = block_start + LZX_ENCODE_BLOCK_SIZE;
        size_t nitems = 0;
        uint32_t block_size;
        uint64_t verbatim_bits;

        if (block_target_end > end) block_target_end = end;

        while (pos < block_target_end) {
            size_t max_len = block_target_end - pos;
            best_match m;

            if (max_len > LZX_MAX_MATCH) max_len = LZX_MAX_MATCH;
            find_best_match(enc, pos, max_len, &m);

            if (m.length >= LZX_MIN_MATCH && !m.is_repeat && m.length < LZX_MAX_MATCH &&
                pos + 1 < block_target_end) {
                /* one-step lazy evaluation for newly-found (non-repeat) matches */
                best_match m2;
                size_t max_len2 = block_target_end - (pos + 1);
                if (max_len2 > LZX_MAX_MATCH) max_len2 = LZX_MAX_MATCH;
                find_best_match(enc, pos + 1, max_len2, &m2);
                if (m2.length > m.length) m.length = 0; /* prefer literal now */
            }

            if (m.length >= LZX_MIN_MATCH) {
                int slot;
                lzx_item *it = &enc->items[nitems++];
                it->is_match = 1;

                if (m.is_repeat) {
                    slot = m.repeat_slot;
                    it->footer_bits = 0;
                    it->footer_value = 0;
                    if (slot == 1) { enc->r1 = enc->r0; enc->r0 = m.distance; }
                    else if (slot == 2) { enc->r2 = enc->r0; enc->r0 = m.distance; }
                } else {
                    uint32_t extra;
                    slot = find_slot_for_distance(m.distance, enc->num_slots);
                    extra = lzx_extra_bits[slot];
                    it->footer_bits = (uint8_t)extra;
                    it->footer_value = m.distance - lzx_position_base[slot];
                    enc->r2 = enc->r1; enc->r1 = enc->r0; enc->r0 = m.distance;
                }

                if (m.length <= 8) {
                    it->length_header = (uint8_t)(m.length - 2);
                } else {
                    it->length_header = 7;
                    it->length_symbol = (uint16_t)(m.length - 9);
                }
                it->main_symbol = (uint16_t)(LZX_NUM_CHARS + slot * 8 + it->length_header);

                insert_range(enc, pos, pos + m.length);
                pos += m.length;
            } else {
                lzx_item *it = &enc->items[nitems++];
                it->is_match = 0;
                it->literal = enc->history[pos];
                insert_position(enc, pos);
                pos += 1;
            }
        }

        block_size = (uint32_t)(pos - block_start);
        verbatim_bits = prepare_verbatim_tables(enc, enc->items, nitems);

        if (verbatim_bits + 2000 >= (uint64_t)block_size * 8) {
            write_block_uncompressed(&bw, enc, block_start, block_size);
            /* prev_len purposefully left untouched, matching original semantics */
        } else {
            uint8_t aligned_len[LZX_ALIGNED_TREE_SYMBOLS];
            uint16_t aligned_codes[LZX_ALIGNED_TREE_SYMBOLS];
            int64_t aligned_delta = prepare_aligned_tree(enc->items, nitems, aligned_len, aligned_codes);

            if (aligned_delta < 0) {
                write_block_aligned(&bw, enc, enc->items, nitems, block_size,
                                     aligned_len, aligned_codes, main_prev_len, len_prev_len);
            } else {
                write_block_verbatim(&bw, enc, enc->items, nitems, block_size, main_prev_len, len_prev_len);
            }
        }
    }

    lzx_bw_align(&bw);
    if (bw.overflow) return XLZX_ERROR_BUFFER_TOO_SMALL;

    *out_size = lzx_bw_bytes_written(&bw, (const uint8_t *)dst);
    return XLZX_OK;
}
