// Re-implementation of a 2011 homebrew tool ("krnlupdate v1.0 - oscar193")
// whose source was lost - reconstructed from a disassembly of the original
// krnlupdate.exe. The original used LibTomCrypt (HMAC-SHA1 + a PRNG-style
// RC4); this version uses ExCrypt's equivalents instead:
//   - LibTomCrypt hmac_init/hmac_process/hmac_done  -> ExCryptHmacSha*
//   - LibTomCrypt rc4_start/rc4_add_entropy/rc4_ready/rc4_read/rc4_done
//         -> ExCryptRc4 (see ../RC4.md for the full mapping)
//
// xboxupd.bin layout (as parsed by the original tool):
//   [0x00] "CF"           - stage 1 (CB) header magic
//   [0x0C] uint32 be      - offset from file start to the "CG" header
//   [0x20] uint8[0x10]    - stage 1 salt
//   [0x30] ...            - stage 1 encrypted body, up to the CG offset
//   CG header (at the offset read above):
//   [+0x00] "CG"          - stage 2 header magic
//   [+0x0C] uint32 be     - total size of the CG section (header+salt+body)
//   [+0x10] uint8[0x10]   - stage 2 salt
//   [+0x20] ...           - stage 2 encrypted body
//
// Stage 1 key = HMAC-SHA1(cpuRomKey, salt) truncated to 16 bytes.
// Stage 2 key = HMAC-SHA1(key @ fixed file offset 0x330, salt) truncated to
// 16 bytes. NOTE: the original tool hardcoded this key's location instead of
// chaining the stage-1 digest, which only works when the CB section is
// exactly 0x330 bytes (true for the xboxupd.bin the original tool targeted).
//
// The LZX-compressed delta patch that follows (applied against the base
// kernel to reconstruct the final xboxkrnl.exe) is intentionally not
// implemented here.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <memory>
#include <iostream>
#include <fstream>
#include "../endian.h"
#include "../crypto/keys.h"
#include "../third_party/ExCrypt/src/excrypt.h"

namespace {

// Equivalent of the original tool's `do_hash`: HMAC-SHA1(key, in[0x10]),
// truncated to a 16-byte digest.
void do_hash(const uint8_t* key, const uint8_t* in, uint8_t* out) {
    EXCRYPT_HMACSHA_STATE state{ 0 };
    ExCryptHmacShaInit(&state, key, 0x10);
    ExCryptHmacShaUpdate(&state, in, 0x10);
    ExCryptHmacShaFinal(&state, out, 0x10);
}

// Equivalent of the original tool's `do_rc4`: RC4-crypt `buf` in place with
// a 16-byte key.
void do_rc4(const uint8_t* key, uint8_t* buf, uint32_t buf_size) {
    ExCryptRc4(key, 0x10, buf, buf_size);
}

uint32_t read_be32(const uint8_t* p) {
    uint32_t value;
    memcpy(&value, p, sizeof(value));
    return xe::endian::big_endian_to_native(value);
}

uint16_t read_be16(const uint8_t* p) {
    uint16_t value;
    memcpy(&value, p, sizeof(value));
    return xe::endian::big_endian_to_native(value);
}

}  // namespace

int main(const int argc, const char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <xboxupd.bin> <base kernel> <output>" << std::endl;
        return 1;
    }

    const char* updateFile = argv[1];
    const char* baseKernelFile = argv[2];
    const char* outFile = argv[3];

    std::ifstream input(updateFile, std::ios::binary);
    if (!input.is_open()) {
        std::cerr << "Failed to open xboxupd.bin: " << updateFile << std::endl;
        return 1;
    }

    input.seekg(0, std::ios::end);
    const size_t inputSize = static_cast<size_t>(input.tellg());
    input.seekg(0, std::ios::beg);

    if (inputSize < 0x30) {
        std::cerr << "xboxupd.bin is too small" << std::endl;
        return 1;
    }

    auto buffer = std::make_unique<uint8_t[]>(inputSize);
    input.read(reinterpret_cast<char*>(buffer.get()), inputSize);
    input.close();

    uint8_t* buf = buffer.get();

    if (buf[0] != 'C' || buf[1] != 'F') {
        std::cerr << "CF magic incorrect in xboxupd.bin" << std::endl;
        return 1;
    }

    // Offset (relative to the start of the file) of the "CG" header.
    const uint32_t cfNextOffset = read_be32(buf + 0xC);

    if (cfNextOffset < 0x30 || cfNextOffset + 0x10 > inputSize) {
        std::cerr << "CF next-offset out of bounds" << std::endl;
        return 1;
    }

    uint8_t* cg = buf + cfNextOffset;

    if (cg[0] != 'C' || cg[1] != 'G') {
        std::cerr << "CG magic incorrect in xboxupd.bin" << std::endl;
        return 1;
    }

    // Total size of the CG section (header + salt + body).
    const uint32_t cgSectionSize = read_be32(cg + 0xC);

    if (cgSectionSize < 0x20 || cfNextOffset + cgSectionSize > inputSize) {
        std::cerr << "CG section size out of bounds" << std::endl;
        return 1;
    }

    // --- Stage 1 (CB) decryption ---
    uint8_t digest1[0x10]{ 0 };
    do_hash(xe::crypto::keys::cpuRomKey, buf + 0x20, digest1);
    do_rc4(digest1, buf + 0x30, cfNextOffset - 0x30);

    // --- Stage 2 (CG) decryption ---
    uint8_t digest2[0x10]{ 0 };
    do_hash(buf + 0x330, cg + 0x10, digest2);
    do_rc4(digest2, cg + 0x20, cgSectionSize - 0x20);

    // Sanity check: the first 4 (big-endian) bytes of the decrypted CG body
    // should equal the size of the base kernel file.
    std::ifstream baseKernel(baseKernelFile, std::ios::binary);
    if (!baseKernel.is_open()) {
        std::cerr << "Failed to open base kernel: " << baseKernelFile << std::endl;
        return 1;
    }
    baseKernel.seekg(0, std::ios::end);
    const uint32_t baseKernelSize = static_cast<uint32_t>(baseKernel.tellg());
    baseKernel.close();

    const uint32_t decryptedKernelSize = read_be32(cg + 0x20);
    if (decryptedKernelSize != baseKernelSize) {
        std::cerr << "Base kernel length mismatch (decrypted=" << decryptedKernelSize
                   << ", base=" << baseKernelSize << ")" << std::endl;
    } else {
        std::cerr << "Base kernel length OK" << std::endl;
    }

    const int16_t kernelVersion = static_cast<int16_t>(read_be16(cg + 2));
    std::cerr << "Updated kernel version: " << kernelVersion << std::endl;

    // TODO: apply the LZX-compressed delta patch (against the base kernel)
    // that follows the CG header to reconstruct the final xboxkrnl.exe.
    // Not implemented - this currently only decrypts the CB/CG sections.

    std::ofstream output(outFile, std::ios::binary);
    if (!output.is_open()) {
        std::cerr << "Failed to open output file: " << outFile << std::endl;
        return 1;
    }
    output.write(reinterpret_cast<const char*>(buffer.get()), inputSize);
    output.close();

    std::cerr << "Wrote decrypted update to: " << outFile << std::endl;

    return 0;
}
