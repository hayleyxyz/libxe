
# xcompress\_lzx — reimplementation notes

This document describes the work done to reimplement the Xbox 360
"XCompress" LZX compressor/decompressor (the `XMemCompress` /
`XMemDecompress` family of APIs) as a clean, new, standalone C library,
based on studying the decompiled sources found in this workspace.

Everything in this folder (`reimpl/`) is newly written. Nothing here was
copied from the decompiled sources — the original `.c`/`.h`/`.asm` files
elsewhere in the workspace were only *read* to recover the on-disk bitstream
format, not reused as code.

## Goal

> Re-implement the XCompress LZX codec APIs into new `.c`/`.h` files. The
> exact API spec doesn't need to match the original — just the end result
> of the codec (successful compress/decompress round-tripping, plus support
> for delta patches / "window data").

## What was studied

- `Dll.dll.c` / `Dll.dll.h` — Hex-Rays decompilation of the original DLL.
  This was the primary source of truth: Ghidra/IDA had recovered real
  struct field names (e.g. `dec_bitbuf`, `dec_main_tree_len`,
  `enc_last_matchpos_offset`), which made it possible to reconstruct the
  exact algorithms rather than just guessing from raw disassembly.
- `xcompress-dll.c` — function declarations only, low additional value.
- `compression/**/*.asm` — raw disassembly dumps (from a build with private
  symbols, but no algorithmic comments); consulted briefly but the `.c`
  decompilation was far more useful, so these were not exhaustively read.

Key facts recovered from the decompiled source (cross-checked against the
well-documented, standard "LZX" compression format used in Microsoft CAB and
WIM containers, which this Xbox 360 variant matches almost exactly):

- **Block types**: `VERBATIM` (1), `ALIGNED` (2), `UNCOMPRESSED` (3).
- **Bit packing**: bits are consumed MSB-first out of a bit buffer that is
  refilled from the input in 16-bit *little-endian* words.
- **Block header**: 3 bits block type + 24 bits block size (as three 8-bit
  fields).
- **Main tree**: 256 literal symbols + `num_position_slots * 8` match
  symbols. A match symbol encodes a length header (0–7) and a position slot;
  header 7 means the real length comes from a second, 249-symbol "length
  tree" (giving match lengths 2–257 total).
- **Position slots**: a 51-entry table of `(extra_bits, position_base)`
  pairs (standard LZX table), used to turn a "slot number" into a match
  distance. The number of slots needed is derived at runtime from the
  window size. This Xbox variant's table only covers window sizes up to
  2 MiB.
- **Repeated offsets** `R0`/`R1`/`R2` (LZX's classic trick for cheaply
  re-encoding recently-used distances): slot 0/1/2 in a match symbol means
  "reuse a repeated offset" rather than "read a new distance", with a
  specific (non-obvious) swap rule that was verified directly against the
  decompiled decoder.
- **Aligned-offset blocks**: an optional refinement where the low 3 bits of
  a match's position footer are entropy-coded through their own tiny
  8-symbol Huffman tree instead of being sent as raw bits.
- **Tree transmission**: code lengths for the main/length trees are sent
  through a 20-symbol "pretree" with a modulo-17 delta scheme (lengths are
  encoded relative to the previous block's tree) plus 3 run-length codes for
  zero-runs and same-value runs.
- **Uncompressed blocks**: byte-align, store the 3 repeated offsets as raw
  32-bit values, then copy raw bytes (padded to an even count).
- **Delta / "window data" support**: `LZX_EncodeInsertDictionary` /
  `LZX_DecodeInsertDictionary` preload reference ("old version") bytes into
  the sliding window without emitting/consuming any compressed output for
  them, so real data compressed afterwards can reference the preloaded
  bytes as if it were already-seen history. This is the mechanism behind
  the original's `XMemCompressSegmentTD`/`XMemDecompressSegmentTD` "title
  data" delta-patch APIs, and is exactly the feature this reimplementation
  exposes directly (see `xlzx_encoder_set_dictionary` /
  `xlzx_decoder_set_dictionary` below).

Full details are recorded in the repo's local Copilot memory
(`/memories/repo/xcompress-lzx-findings.md`) for anyone continuing this work
with an AI assistant.

## Design decisions / where this intentionally differs from the original

Because bit-exact API/behavioural compatibility wasn't required, several
things were simplified:

- **Different public API names/shapes.** No `XMemCompress`, `HRESULT`,
  `_XMEMCODEC_PARAMETERS_LZX`, etc. — a small, portable C99 API instead (see
  below). No Windows dependency (no `windows.h`, no CryptoAPI).
- **Self-describing container format.** `xlzx_compress()` writes a small
  20-byte header (magic, window size, flags, uncompressed size) so
  `xlzx_decompress()` doesn't need the caller to separately track those —
  unlike the original, which relies on the caller/container to know the
  window size and exact output size up front.
- **No in-stream "E8 translation" bit.** The x86 CALL-address translation
  filter (`XLZX_FLAG_E8_TRANSLATION`) is applied as an independent
  whole-buffer pre/post-processing pass at the top-level API, not woven
  into the block codec itself. Simpler, and it's proven to be exactly
  self-inverse regardless of "false positive" `0xE8` bytes (see the comment
  in `xcompress_lzx.c`).
- **Encoder never emits ALIGNED blocks** (only VERBATIM or UNCOMPRESSED).
  Aligned-offset blocks are a pure compression-ratio optimisation on top of
  an already-valid stream; skipping them on the write side keeps the
  encoder simpler while remaining fully spec-compliant — any conformant LZX
  reader (including this library's own decoder, which *does* support all
  three block types for read compatibility) can still decode the result.
- **Hash-chain + 1-step lazy match finder**, not the original's binary-tree
  optimal parse. Produces valid, reasonably-compressed LZX output; not
  bit-identical to what the original compressor would produce for the same
  input.
- **Length-limited Huffman via a simple "rescale on overflow" loop**
  instead of a from-memory reproduction of the original's exact tree-length
  algorithm — guaranteed to terminate and respect the 16-bit code length
  limit for this codec's alphabet sizes.
- **Canonical Huffman decoding uses a simple table-free "peel one bit at a
  time" scheme** (the same idea as Mark Adler's public-domain `puff.c`
  reference decoder), rather than reverse-engineering the original's
  internal tree-node/negative-index table representation. Only the
  *external* bit↔symbol mapping needs to match the LZX spec; the internal
  representation is free to differ since both the encoder and decoder here
  are new code.
- **Linear, growable history buffers** (realloc, no compaction/circular
  indexing) for both the encoder and decoder, rather than the original's
  fixed-size sliding window with periodic compaction. Simpler; uses more
  memory for very long streaming sessions since old data is never trimmed,
  but match distances are still correctly bounded to the configured window
  size.
- **The "TD segment" protocol is not wire-replicated.** The original's
  `XMemBeginCompressionTD`/`XMemCompressSegmentTD`/`XMemEndCompressionTD` add
  a SHA-1 header hash and segment-pitch bookkeeping on top of the
  dictionary mechanism. This reimplementation exposes the underlying
  dictionary/window-preload primitive directly instead, which achieves the
  same delta-compression *capability* more simply and generically.

## File layout

| File | Contents |
|---|---|
| `xcompress_lzx.h` | Public API (the only header consumers need) |
| `lzx_internal.h` | Shared constants, bit reader/writer, canonical Huffman coding helpers |
| `lzx_tables.c` | LZX position-slot tables (`extra_bits`/`position_base`) |
| `lzx_decoder.c` | Full LZX block decoder (verbatim/aligned/uncompressed) |
| `lzx_encoder.c` | LZX encoder (match finder, Huffman tree building, block writer) |
| `xcompress_lzx.c` | One-shot API, container header, optional E8 filter |
| `test_roundtrip.c` | Test/demo program (not part of the library) |

## Public API summary

```c
// One-shot buffer <-> buffer
size_t xlzx_compress_bound(size_t src_size);
xlzx_result xlzx_compress(const xlzx_params *params,
                           const void *src, size_t src_size,
                           void *dst, size_t dst_capacity, size_t *out_size);
xlzx_result xlzx_decompress(const xlzx_params *params,
                             const void *src, size_t src_size,
                             void *dst, size_t dst_capacity, size_t *out_size);

// Streaming / segmented use (history persists across calls on one context)
xlzx_encoder *xlzx_encoder_create(const xlzx_params *params);
xlzx_result xlzx_encoder_compress(xlzx_encoder *enc, const void *src, size_t src_size,
                                   void *dst, size_t dst_capacity, size_t *out_size);
xlzx_decoder *xlzx_decoder_create(const xlzx_params *params);
xlzx_result xlzx_decoder_decompress(xlzx_decoder *dec, const void *src, size_t src_size,
                                     void *dst, size_t dst_capacity, size_t *out_size);

// Delta / "window data" patching
xlzx_result xlzx_encoder_set_dictionary(xlzx_encoder *enc, const void *data, size_t size);
xlzx_result xlzx_decoder_set_dictionary(xlzx_decoder *dec, const void *data, size_t size);
```

`xlzx_params` has just two fields: `window_size` (power of two, 32 KiB –
2 MiB, defaults to 128 KiB) and `flags` (currently just
`XLZX_FLAG_E8_TRANSLATION`).

## Building and testing

No build system was added (single translation units, trivially compiled
directly). This was built and tested with MinGW-w64 GCC (via MSYS2's
`ucrt64` environment):

```powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
gcc -std=c99 -O2 -Wall -Wextra -Wpedantic -o test_roundtrip.exe `
    lzx_tables.c lzx_decoder.c lzx_encoder.c xcompress_lzx.c test_roundtrip.c
.\test_roundtrip.exe
```

It compiles cleanly (zero warnings with `-Wall -Wextra -Wpedantic`) and all
tests pass. `test_roundtrip.c` exercises:

- Empty buffer, short text, 200 000 random bytes, 500 000 semi-compressible
  bytes.
- The E8 translation filter on data containing many `0xE8` bytes.
- Minimum (32 KiB) and maximum (2 MiB) window sizes.
- Dictionary/delta compression: a 301-byte "updated" buffer compresses to
  88 bytes against a similar reference buffer used as a dictionary, versus
  318 bytes compressed plain (no dictionary) — and decompresses back to the
  exact original using the same dictionary.
- Multi-call streaming: 5 chunks compressed/decompressed across repeated
  calls on the same encoder/decoder context, all restored exactly.
- Invalid-parameter rejection (non-power-of-two, below-minimum, and
  above-maximum window sizes).
- The ALIGNED block type was also explicitly (temporarily) forced on to
  verify that decode path, since the test data didn't naturally trigger the
  encoder's cost-based decision to use it.

## Known limitations / possible future work

- Compression ratio is good but not optimal (no optimal parsing).
- No wire-exact replication of the original `XMemCompressSegmentTD` header
  format (SHA-1 hash, segment pitch) — use `xlzx_*_set_dictionary` instead,
  which provides the same delta-compression capability.
- No periodic history compaction — long-lived streaming contexts retain all
  history in memory (bounded compression *distance* is still correctly
  enforced, just not bounded *memory use*).
