#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>

#include <excrypt.h>
#include "../mspack_lzx/mspack_lzx.h"

constexpr size_t block_size = 0x8000;
constexpr uint32_t window_size = 0x20000u; // 128 KiB - matches original XMEMCODEC_PARAMETERS_LZX default


#include "compressed.h"
constexpr size_t total_compressed_size = sizeof(blocks);
constexpr size_t total_uncompressed_size = 0x260000;

namespace {

bool read_file(const char *path, std::vector<uint8_t> *out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        fprintf(stderr, "failed to open '%s'\n", path);
        return false;
    }
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    out->resize(static_cast<size_t>(size));
    if (size > 0 && !f.read(reinterpret_cast<char *>(out->data()), size)) {
        fprintf(stderr, "failed to read '%s'\n", path);
        return false;
    }
    return true;
}

// Decompresses an LZX DELTA patch (e.g. an Xbox 360 xboxupd.bin-style
// kernel/dashboard patch) against its base/reference file.
//
// Usage: lzxtool delta <base_file> <delta_file> <header_skip> <output_size>
//        <output_file> [window_size]
//
// - <base_file>    the original, unpatched file the delta was generated
//                   against (e.g. xboxkrnl.1888.exe).
// - <delta_file>   the LZX DELTA-compressed patch (e.g. xboxupd.bin). Its
//                  own container header is skipped over with <header_skip>
//                  bytes before the raw LZX DELTA bitstream begins.
// - <header_skip>  number of bytes at the start of <delta_file> to skip
//                  before the LZX DELTA data starts (decimal or 0x.. hex).
// - <output_size>  exact expected decompressed size in bytes (decimal or
//                  0x.. hex) - LZX DELTA doesn't self-terminate, so the
//                  caller must know/guess this up front. A patched kernel
//                  is usually close to the same size as the base file, so
//                  that's a reasonable first guess if the real size isn't
//                  known.
// - <output_file>  where to write the decompressed result.
// - [window_size]  optional LZX DELTA window size in bytes, must be a
//                  power of two between 0x20000 (128 KiB) and 0x2000000
//                  (32 MiB). Defaults to 0x20000, the historical Xbox 360
//                  XMEMCODEC_PARAMETERS_LZX default.
int run_delta(int argc, char *argv[]) {
    if (argc < 7) {
        fprintf(stderr,
                "usage: %s delta <base_file> <delta_file> <header_skip> <output_size> <output_file> "
                "[window_size]\n",
                argv[0]);
        return 1;
    }

    const char *base_path = argv[2];
    const char *delta_path = argv[3];
    size_t header_skip = static_cast<size_t>(strtoull(argv[4], nullptr, 0));
    size_t expected_output_size = static_cast<size_t>(strtoull(argv[5], nullptr, 0));
    const char *output_path = argv[6];
    uint32_t delta_window_size = argc > 7 ? static_cast<uint32_t>(strtoul(argv[7], nullptr, 0)) : window_size;

    std::vector<uint8_t> base;
    std::vector<uint8_t> delta;
    if (!read_file(base_path, &base) || !read_file(delta_path, &delta)) {
        return 1;
    }
    if (header_skip >= delta.size()) {
        fprintf(stderr, "header_skip (0x%zx) is past the end of '%s' (0x%zx bytes)\n", header_skip, delta_path,
                delta.size());
        return 1;
    }

    // The LZX DELTA reference/dictionary window is preloaded with the tail
    // of the base file - i.e. the bytes treated as if they immediately
    // precede byte 0 of the decompressed output - zero-padded at the front
    // if the base file is smaller than the window.
    size_t window_data_len = std::min(static_cast<size_t>(delta_window_size), base.size());
    const uint8_t *window_data = base.data() + (base.size() - window_data_len);

    const uint8_t *lzx_data = delta.data() + header_skip;
    size_t lzx_data_size = delta.size() - header_skip;

    std::vector<uint8_t> output(expected_output_size);
    size_t out_size = 0;
    auto result = mspack_lzx_decompress(lzx_data, lzx_data_size, output.data(), output.size(), delta_window_size,
                                         window_data, window_data_len, &out_size, /*is_delta=*/true);

    fprintf(stderr, "LZX DELTA result: %d (%s)\n", result, result == MSPACK_LZX_OK ? "OK" : "ERROR");
    if (result != MSPACK_LZX_OK) {
        fprintf(stderr,
                "hint: double check --header_skip (currently 0x%zx) and --output_size (currently 0x%zx) - "
                "LZX DELTA has no self-describing length, both must be exactly right\n",
                header_skip, expected_output_size);
        return 1;
    }

    std::ofstream out(output_path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(output.data()), static_cast<std::streamsize>(out_size));
    fprintf(stderr, "wrote 0x%zx bytes to '%s'\n", out_size, output_path);
    return 0;
}

int run_demo() {

    unsigned char* uncompressed = (unsigned char*)malloc(total_uncompressed_size);

    size_t out_size = 0;
    auto result = mspack_lzx_decompress(blocks, total_compressed_size, uncompressed, total_uncompressed_size,
        window_size, nullptr, 0, &out_size);

    fprintf(stderr, "LZX (one-shot) result: %d (%s)\n", result, result == MSPACK_LZX_OK ? "OK" : "ERROR");

    // Exercise the streaming API: feed all compressed input up front (since
    // this test dataset doesn't carry per-block compressed sizes to chunk
    // it by), but pull decompressed output out 32KB frame at a time across
    // multiple calls, to prove the decoder correctly resumes state between
    // decompress() calls.
    unsigned char* streamed = (unsigned char*)malloc(total_uncompressed_size);
    auto decoder = mspack_lzx_decoder_create(window_size, nullptr, 0);
    mspack_lzx_decoder_set_output_length(decoder, total_uncompressed_size);
    mspack_lzx_decoder_feed(decoder, blocks, total_compressed_size);

    mspack_lzx_result stream_result = MSPACK_LZX_OK;
    for (size_t off = 0; off < total_uncompressed_size && stream_result == MSPACK_LZX_OK; off += block_size) {
        size_t chunk_size = std::min(block_size, total_uncompressed_size - off);
        size_t produced = 0;
        stream_result = mspack_lzx_decoder_decompress(decoder, streamed + off, chunk_size, &produced);
    }
    mspack_lzx_decoder_destroy(decoder);

    bool streamed_matches = stream_result == MSPACK_LZX_OK &&
        std::memcmp(uncompressed, streamed, total_uncompressed_size) == 0;
    fprintf(stderr, "LZX (streaming) result: %d (%s), matches one-shot output: %s\n", stream_result,
        stream_result == MSPACK_LZX_OK ? "OK" : "ERROR", streamed_matches ? "yes" : "no");

    return 0;
}

}  // namespace

int main(int argc, char *argv[]) {
    if (argc > 1 && std::strcmp(argv[1], "delta") == 0) {
        return run_delta(argc, argv);
    }
    return run_demo();
}
