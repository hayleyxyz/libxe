/*
 * xcompress_lzx.c - public one-shot API + small self-describing container
 * format on top of the core LZX encoder/decoder (lzx_encoder.c /
 * lzx_decoder.c).
 *
 * Container layout written by xlzx_compress() / read by xlzx_decompress():
 *   offset  0: uint32 magic ('X','L','Z','1')
 *   offset  4: uint32 window_size (little-endian)
 *   offset  8: uint32 flags (little-endian)
 *   offset 12: uint64 uncompressed_size (little-endian)
 *   offset 20: raw LZX block stream (see lzx_decoder.c)
 *
 * This header is a convenience added by this reimplementation (the
 * original XMemCompress relies on the caller tracking window size/output
 * size separately) so a compressed buffer is fully self-describing.
 */
#include "xcompress_lzx.h"
#include "lzx_internal.h"

#define XLZX_MAGIC 0x315A4C58u /* "XLZ1" little-endian in memory */
#define XLZX_HEADER_SIZE 20u
#define XLZX_ENCODE_BLOCK_SIZE_HINT 32768u

static void put_u32le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void put_u64le(uint8_t *p, uint64_t v) {
    int i;
    for (i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i));
}
static uint32_t get_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t get_u64le(const uint8_t *p) {
    uint64_t v = 0;
    int i;
    for (i = 0; i < 8; i++) v |= (uint64_t)p[i] << (8 * i);
    return v;
}

/* Reversible whole-buffer x86 CALL (0xE8 rel32) address translation filter.
 * Applied unconditionally to every 0xE8 byte found (no heuristic range
 * check) - this keeps the filter trivially self-inverse: the same linear
 * scan (advancing 5 bytes whenever an 0xE8 "opcode" is seen, 1 byte
 * otherwise) visits exactly the same set of trigger positions whether
 * applied forwards or backwards, because a transform only ever rewrites the
 * 4 bytes strictly after its own trigger byte, which the scan never
 * revisits as a new trigger candidate. */
static void e8_translate(uint8_t *data, size_t size, int forward) {
    size_t i;
    if (size < 5) return;
    for (i = 0; i + 5 <= size; i++) {
        if (data[i] == 0xE8) {
            uint32_t operand = get_u32le(data + i + 1);
            uint32_t pos = (uint32_t)i;
            uint32_t result = forward ? (operand + pos + 5) : (operand - pos - 5);
            put_u32le(data + i + 1, result);
            i += 4;
        }
    }
}

size_t xlzx_compress_bound(size_t src_size) {
    size_t blocks = (src_size + XLZX_ENCODE_BLOCK_SIZE_HINT - 1) / XLZX_ENCODE_BLOCK_SIZE_HINT;
    if (blocks == 0) blocks = 1;
    return XLZX_HEADER_SIZE + src_size + blocks * 24 + 64;
}

xlzx_result xlzx_compress(const xlzx_params *params,
                           const void *src, size_t src_size,
                           void *dst, size_t dst_capacity, size_t *out_size) {
    xlzx_params p;
    xlzx_encoder *enc;
    uint8_t *filtered = NULL;
    const void *effective_src = src;
    xlzx_result rc;
    size_t compressed_size = 0;

    if ((src_size > 0 && !src) || !dst || !out_size) return XLZX_ERROR_INVALID_PARAMETER;

    p.window_size = params && params->window_size ? params->window_size : XLZX_WINDOW_SIZE_DEFAULT;
    p.flags = params ? params->flags : 0;

    if (dst_capacity < XLZX_HEADER_SIZE) return XLZX_ERROR_BUFFER_TOO_SMALL;

    if (p.flags & XLZX_FLAG_E8_TRANSLATION) {
        if (src_size > 0) {
            filtered = (uint8_t *)malloc(src_size);
            if (!filtered) return XLZX_ERROR_OUT_OF_MEMORY;
            memcpy(filtered, src, src_size);
            e8_translate(filtered, src_size, 1);
        }
        effective_src = filtered;
    }

    enc = xlzx_encoder_create(&p);
    if (!enc) { free(filtered); return XLZX_ERROR_INVALID_PARAMETER; }

    rc = xlzx_encoder_compress(enc, effective_src, src_size,
                                (uint8_t *)dst + XLZX_HEADER_SIZE, dst_capacity - XLZX_HEADER_SIZE,
                                &compressed_size);
    xlzx_encoder_destroy(enc);
    free(filtered);
    if (rc != XLZX_OK) return rc;

    put_u32le((uint8_t *)dst, XLZX_MAGIC);
    put_u32le((uint8_t *)dst + 4, p.window_size);
    put_u32le((uint8_t *)dst + 8, p.flags);
    put_u64le((uint8_t *)dst + 12, (uint64_t)src_size);

    *out_size = XLZX_HEADER_SIZE + compressed_size;
    return XLZX_OK;
}

xlzx_result xlzx_decompress(const xlzx_params *params,
                             const void *src, size_t src_size,
                             void *dst, size_t dst_capacity, size_t *out_size) {
    const uint8_t *bytes = (const uint8_t *)src;
    xlzx_params p;
    xlzx_decoder *dec;
    uint64_t uncompressed_size;
    xlzx_result rc;
    size_t decoded_size = 0;
    (void)params;

    if (!src || src_size < XLZX_HEADER_SIZE || !out_size) return XLZX_ERROR_INVALID_PARAMETER;
    if (get_u32le(bytes) != XLZX_MAGIC) return XLZX_ERROR_CORRUPT_DATA;

    p.window_size = get_u32le(bytes + 4);
    p.flags = get_u32le(bytes + 8);
    uncompressed_size = get_u64le(bytes + 12);

    if (uncompressed_size > dst_capacity) return XLZX_ERROR_BUFFER_TOO_SMALL;

    dec = xlzx_decoder_create(&p);
    if (!dec) return XLZX_ERROR_CORRUPT_DATA;

    rc = xlzx_decoder_decompress(dec, bytes + XLZX_HEADER_SIZE, src_size - XLZX_HEADER_SIZE,
                                  dst, dst_capacity, &decoded_size);
    xlzx_decoder_destroy(dec);
    if (rc != XLZX_OK) return rc;
    if (decoded_size != uncompressed_size) return XLZX_ERROR_CORRUPT_DATA;

    if (p.flags & XLZX_FLAG_E8_TRANSLATION) {
        e8_translate((uint8_t *)dst, decoded_size, 0);
    }

    *out_size = decoded_size;
    return XLZX_OK;
}
