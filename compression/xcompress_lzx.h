/*
 * xcompress_lzx.h - public API for this reimplementation of the Xbox 360
 * "XCompress" LZX compressor/decompressor (XMemCompress / XMemDecompress
 * family).
 *
 * This is a from-scratch reimplementation derived from studying decompiled
 * sources of the original DLL. It produces/consumes a standard LZX
 * bitstream (compatible block types, Huffman trees, position slots, etc.)
 * wrapped in a small custom container header so that a compressed buffer is
 * self-describing (no need for the caller to separately track window size
 * or uncompressed size). The function names/signatures intentionally differ
 * from the original XMem* API - only the codec behaviour (successful
 * compress/decompress round-tripping, plus delta/dictionary support) is
 * preserved.
 *
 * Three ways to use this library:
 *
 *   1. One-shot buffer compression:  xlzx_compress() / xlzx_decompress()
 *   2. Streaming / segmented use, keeping history across multiple calls:
 *      xlzx_encoder_* / xlzx_decoder_*
 *   3. Delta ("title data") patching against a reference buffer: create an
 *      encoder/decoder, call xlzx_*_set_dictionary() with the reference
 *      ("old") data, then compress/decompress the new data - matches can
 *      reference the dictionary so similar data compresses very well, and
 *      the dictionary bytes themselves are never emitted in the output.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XLZX_WINDOW_SIZE_MIN     0x8000u     /* 32 KiB */
#define XLZX_WINDOW_SIZE_MAX     0x200000u   /* 2 MiB */
#define XLZX_WINDOW_SIZE_DEFAULT 0x20000u    /* 128 KiB - same default as the original XMEMCODEC_PARAMETERS_LZX */

typedef enum xlzx_result {
    XLZX_OK                        = 0,
    XLZX_ERROR_INVALID_PARAMETER   = -1,
    XLZX_ERROR_OUT_OF_MEMORY       = -2,
    XLZX_ERROR_BUFFER_TOO_SMALL    = -3,
    XLZX_ERROR_CORRUPT_DATA        = -4
} xlzx_result;

enum xlzx_flags {
    /* Apply a reversible x86 CALL (0xE8) address translation filter before
     * compressing / after decompressing. Improves compression of x86 code,
     * neutral-to-harmless for other data. Off by default. */
    XLZX_FLAG_E8_TRANSLATION = 1u << 0
};

typedef struct xlzx_params {
    uint32_t window_size; /* power of two in [XLZX_WINDOW_SIZE_MIN, XLZX_WINDOW_SIZE_MAX]; 0 = default */
    uint32_t flags;       /* bitwise-or of xlzx_flags */
} xlzx_params;

typedef struct xlzx_encoder xlzx_encoder;
typedef struct xlzx_decoder xlzx_decoder;

/* ------------------------------------------------------------------ */
/* One-shot buffer <-> buffer helpers                                   */
/* ------------------------------------------------------------------ */

/* Upper bound on the compressed size for `src_size` bytes of input; use to
 * size the destination buffer passed to xlzx_compress(). */
size_t xlzx_compress_bound(size_t src_size);

xlzx_result xlzx_compress(const xlzx_params *params,
                           const void *src, size_t src_size,
                           void *dst, size_t dst_capacity, size_t *out_size);

/* `params` is optional here (may be NULL) - window size/flags are read back
 * from the container header written by xlzx_compress(). */
xlzx_result xlzx_decompress(const xlzx_params *params,
                             const void *src, size_t src_size,
                             void *dst, size_t dst_capacity, size_t *out_size);

/* ------------------------------------------------------------------ */
/* Streaming / delta encoder                                           */
/* ------------------------------------------------------------------ */

xlzx_encoder *xlzx_encoder_create(const xlzx_params *params);
void xlzx_encoder_destroy(xlzx_encoder *enc);

/* Reset all compression state (as if freshly created) but keep buffers
 * allocated. */
xlzx_result xlzx_encoder_reset(xlzx_encoder *enc);

/* Preload `size` bytes of reference ("old version") data into the sliding
 * window without emitting any compressed output for it. Must be called
 * before the first xlzx_encoder_compress() call (or right after a reset).
 * `size` must not exceed the encoder's window size. */
xlzx_result xlzx_encoder_set_dictionary(xlzx_encoder *enc, const void *data, size_t size);

/* Compresses one chunk of new input, continuing from whatever history/
 * dictionary the encoder already has. Produces a self-contained, byte-
 * aligned compressed chunk (NOT a full xlzx_compress() container - just the
 * raw block stream for this call); the matching sequence of
 * xlzx_decoder_decompress() calls on an equivalently-seeded decoder
 * reconstructs the original data. */
xlzx_result xlzx_encoder_compress(xlzx_encoder *enc,
                                   const void *src, size_t src_size,
                                   void *dst, size_t dst_capacity, size_t *out_size);

/* ------------------------------------------------------------------ */
/* Streaming / delta decoder                                           */
/* ------------------------------------------------------------------ */

xlzx_decoder *xlzx_decoder_create(const xlzx_params *params);
void xlzx_decoder_destroy(xlzx_decoder *dec);

xlzx_result xlzx_decoder_reset(xlzx_decoder *dec);

/* Must mirror xlzx_encoder_set_dictionary() exactly (same bytes, same size)
 * for the corresponding compressed chunk to decode correctly. */
xlzx_result xlzx_decoder_set_dictionary(xlzx_decoder *dec, const void *data, size_t size);

/* Decompresses exactly one chunk previously produced by
 * xlzx_encoder_compress(). `src_size` must be the exact compressed size of
 * that chunk. */
xlzx_result xlzx_decoder_decompress(xlzx_decoder *dec,
                                     const void *src, size_t src_size,
                                     void *dst, size_t dst_capacity, size_t *out_size);

#ifdef __cplusplus
}
#endif
