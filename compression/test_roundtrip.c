/*
 * test_roundtrip.c - self-test / demo for the xcompress_lzx reimplementation.
 *
 * Not part of the library; just exercises the public API:
 *   - one-shot compress/decompress on text and random binary data
 *   - dictionary/delta compression against a "reference" buffer
 *   - multi-call streaming compression
 *   - the optional E8 translation filter
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xcompress_lzx.h"

static int g_failures = 0;

static void check(int cond, const char *what) {
    if (!cond) {
        printf("  [FAIL] %s\n", what);
        g_failures++;
    } else {
        printf("  [ok]   %s\n", what);
    }
}

static unsigned char *make_random_buffer(size_t size, unsigned int seed) {
    unsigned char *buf = (unsigned char *)malloc(size);
    size_t i;
    unsigned int state = seed;
    for (i = 0; i < size; i++) {
        state = state * 1103515245u + 12345u;
        buf[i] = (unsigned char)(state >> 16);
    }
    return buf;
}

/* Semi-compressible: mix of repeats and randomness, closer to real-world data. */
static unsigned char *make_semi_compressible_buffer(size_t size, unsigned int seed) {
    unsigned char *buf = (unsigned char *)malloc(size);
    size_t i;
    unsigned int state = seed;
    for (i = 0; i < size; i++) {
        if ((i % 97) < 40) {
            buf[i] = (unsigned char)('A' + (i % 7));
        } else {
            state = state * 1103515245u + 12345u;
            buf[i] = (unsigned char)(state >> 16);
        }
    }
    return buf;
}

static void test_oneshot(const char *label, const unsigned char *data, size_t size,
                          uint32_t window_size, uint32_t flags) {
    xlzx_params params;
    size_t bound, compressed_size = 0, decompressed_size = 0;
    unsigned char *compressed, *decompressed;
    xlzx_result rc;

    printf("test_oneshot: %s (size=%zu, window=0x%x, flags=%u)\n",
           label, size, (unsigned)window_size, (unsigned)flags);

    params.window_size = window_size;
    params.flags = flags;

    bound = xlzx_compress_bound(size);
    compressed = (unsigned char *)malloc(bound);
    decompressed = (unsigned char *)malloc(size ? size : 1);

    rc = xlzx_compress(&params, data, size, compressed, bound, &compressed_size);
    check(rc == XLZX_OK, "compress returns XLZX_OK");

    rc = xlzx_decompress(NULL, compressed, compressed_size, decompressed, size, &decompressed_size);
    check(rc == XLZX_OK, "decompress returns XLZX_OK");
    check(decompressed_size == size, "decompressed size matches original");
    check(size == 0 || memcmp(data, decompressed, size) == 0, "decompressed data matches original");

    if (size > 0) {
        printf("  compressed %zu -> %zu bytes (%.1f%%)\n", size, compressed_size,
               100.0 * (double)compressed_size / (double)size);
    }

    free(compressed);
    free(decompressed);
}

static void test_dictionary_delta(void) {
    const char *reference =
        "The quick brown fox jumps over the lazy dog. "
        "The quick brown fox jumps over the lazy dog. "
        "This is a reference buffer that represents an OLD version of a file. "
        "It contains a lot of repeated filler text so that delta compression "
        "against it is actually meaningful to test.";
    const char *updated =
        "The quick brown fox jumps over the lazy dog. "
        "The quick brown fox jumps over the lazy dog. "
        "This is a reference buffer that represents a NEW version of a file! "
        "It contains a lot of repeated filler text so that delta compression "
        "against it is actually meaningful to test, with an extra sentence appended.";

    size_t ref_size = strlen(reference);
    size_t upd_size = strlen(updated);
    size_t bound = xlzx_compress_bound(upd_size);
    unsigned char *delta = (unsigned char *)malloc(bound);
    unsigned char *plain = (unsigned char *)malloc(bound);
    unsigned char *restored = (unsigned char *)malloc(upd_size + 1);
    size_t delta_size = 0, plain_size = 0, restored_size = 0;
    xlzx_params params;
    xlzx_encoder *enc;
    xlzx_decoder *dec;
    xlzx_result rc;

    printf("test_dictionary_delta\n");

    params.window_size = XLZX_WINDOW_SIZE_DEFAULT;
    params.flags = 0;

    /* Delta compression using the reference buffer as a preset dictionary. */
    enc = xlzx_encoder_create(&params);
    check(enc != NULL, "encoder_create");
    rc = xlzx_encoder_set_dictionary(enc, reference, ref_size);
    check(rc == XLZX_OK, "encoder_set_dictionary");
    rc = xlzx_encoder_compress(enc, updated, upd_size, delta, bound, &delta_size);
    check(rc == XLZX_OK, "encoder_compress (delta)");
    xlzx_encoder_destroy(enc);

    /* Plain compression without the dictionary, for comparison. */
    enc = xlzx_encoder_create(&params);
    rc = xlzx_encoder_compress(enc, updated, upd_size, plain, bound, &plain_size);
    check(rc == XLZX_OK, "encoder_compress (plain, no dictionary)");
    xlzx_encoder_destroy(enc);

    printf("  delta-compressed size = %zu, plain-compressed size = %zu (source %zu bytes)\n",
           delta_size, plain_size, upd_size);
    check(delta_size <= plain_size, "delta compression is at least as small as plain compression");

    /* Decode the delta using the same dictionary. */
    dec = xlzx_decoder_create(&params);
    check(dec != NULL, "decoder_create");
    rc = xlzx_decoder_set_dictionary(dec, reference, ref_size);
    check(rc == XLZX_OK, "decoder_set_dictionary");
    rc = xlzx_decoder_decompress(dec, delta, delta_size, restored, upd_size, &restored_size);
    check(rc == XLZX_OK, "decoder_decompress (delta)");
    check(restored_size == upd_size, "restored size matches updated size");
    check(memcmp(restored, updated, upd_size) == 0, "restored data matches updated data exactly");
    xlzx_decoder_destroy(dec);

    free(delta);
    free(plain);
    free(restored);
}

static void test_streaming(void) {
    xlzx_params params;
    xlzx_encoder *enc;
    xlzx_decoder *dec;
    const int nchunks = 5;
    const size_t chunk_size = 9000;
    unsigned char *chunks[5];
    unsigned char *compressed[5];
    unsigned char *restored[5];
    size_t compressed_sizes[5];
    size_t restored_sizes[5];
    size_t bound = xlzx_compress_bound(chunk_size);
    int i;
    int all_ok = 1;

    printf("test_streaming (multi-call, shared history across calls)\n");

    params.window_size = XLZX_WINDOW_SIZE_DEFAULT;
    params.flags = 0;

    enc = xlzx_encoder_create(&params);
    dec = xlzx_decoder_create(&params);
    check(enc != NULL && dec != NULL, "encoder/decoder create");

    for (i = 0; i < nchunks; i++) {
        /* repeat a pattern across chunks so later chunks can reference earlier ones */
        chunks[i] = make_semi_compressible_buffer(chunk_size, 1000u + (unsigned)i);
        compressed[i] = (unsigned char *)malloc(bound);
        restored[i] = (unsigned char *)malloc(chunk_size);

        if (xlzx_encoder_compress(enc, chunks[i], chunk_size, compressed[i], bound, &compressed_sizes[i]) != XLZX_OK) {
            all_ok = 0;
        }
        if (xlzx_decoder_decompress(dec, compressed[i], compressed_sizes[i], restored[i], chunk_size, &restored_sizes[i]) != XLZX_OK) {
            all_ok = 0;
        }
    }
    check(all_ok, "all chunks compressed+decompressed without error");

    all_ok = 1;
    for (i = 0; i < nchunks; i++) {
        if (restored_sizes[i] != chunk_size || memcmp(restored[i], chunks[i], chunk_size) != 0) {
            all_ok = 0;
        }
    }
    check(all_ok, "all restored chunks match originals exactly");

    xlzx_encoder_destroy(enc);
    xlzx_decoder_destroy(dec);
    for (i = 0; i < nchunks; i++) {
        free(chunks[i]);
        free(compressed[i]);
        free(restored[i]);
    }
}

static void test_invalid_params(void) {
    xlzx_params bad;
    printf("test_invalid_params\n");

    bad.window_size = 0x9000; /* not a power of two */
    bad.flags = 0;
    check(xlzx_encoder_create(&bad) == NULL, "encoder_create rejects non-power-of-two window size");
    check(xlzx_decoder_create(&bad) == NULL, "decoder_create rejects non-power-of-two window size");

    bad.window_size = 0x4000; /* below minimum */
    check(xlzx_encoder_create(&bad) == NULL, "encoder_create rejects window size below minimum");

    bad.window_size = 0x400000; /* above maximum */
    check(xlzx_encoder_create(&bad) == NULL, "encoder_create rejects window size above maximum");
}

int main(void) {
    const char *text =
        "This is a small piece of text used to sanity-check the LZX "
        "reimplementation. Repeat, repeat, repeat, repeat, repeat, repeat. "
        "The quick brown fox jumps over the lazy dog. "
        "The quick brown fox jumps over the lazy dog. "
        "The quick brown fox jumps over the lazy dog.";
    unsigned char *random_buf;
    unsigned char *e8_buf;
    unsigned char *compressible_buf;
    size_t i;

    printf("=== xcompress_lzx round-trip tests ===\n\n");

    test_oneshot("empty buffer", (const unsigned char *)"", 0, XLZX_WINDOW_SIZE_DEFAULT, 0);
    test_oneshot("short text", (const unsigned char *)text, strlen(text), XLZX_WINDOW_SIZE_DEFAULT, 0);

    random_buf = make_random_buffer(200000, 42);
    test_oneshot("200000 random bytes", random_buf, 200000, XLZX_WINDOW_SIZE_DEFAULT, 0);
    free(random_buf);

    compressible_buf = make_semi_compressible_buffer(500000, 7);
    test_oneshot("500000 semi-compressible bytes", compressible_buf, 500000, XLZX_WINDOW_SIZE_DEFAULT, 0);
    free(compressible_buf);

    /* buffer with plenty of 0xE8 bytes to exercise the translation filter */
    e8_buf = (unsigned char *)malloc(100000);
    for (i = 0; i < 100000; i++) {
        e8_buf[i] = (unsigned char)((i % 13 == 0) ? 0xE8 : (i * 37) & 0xFF);
    }
    test_oneshot("100000 bytes with E8 translation", e8_buf, 100000, XLZX_WINDOW_SIZE_DEFAULT, XLZX_FLAG_E8_TRANSLATION);
    free(e8_buf);

    /* window size boundaries */
    {
        unsigned char *small_win_buf = make_semi_compressible_buffer(60000, 11);
        unsigned char *big_win_buf = make_semi_compressible_buffer(600000, 13);
        test_oneshot("min window size (0x8000)", small_win_buf, 60000, XLZX_WINDOW_SIZE_MIN, 0);
        test_oneshot("max window size (0x200000)", big_win_buf, 600000, XLZX_WINDOW_SIZE_MAX, 0);
        free(small_win_buf);
        free(big_win_buf);
    }

    printf("\n");
    test_dictionary_delta();

    printf("\n");
    test_streaming();

    printf("\n");
    test_invalid_params();

    printf("\n=== %s ===\n", g_failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return g_failures == 0 ? 0 : 1;
}
