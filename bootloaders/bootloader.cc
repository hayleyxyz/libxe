//
// Created by github.com/hayleyxyz on 29/10/2018.
//

#include <iostream>
#include <cassert>
#include "bootloader.h"
#include "../io/memory_stream.h"
#include <excrypt.h>
#include "../compression/xcompress_lzx.h"

namespace xe {
namespace bootloaders {

void Bootloader::read(xe::io::Stream &stream) {
    offset = stream.position();

    magic = stream.readIntBE<uint16_t>();
    version = stream.readIntBE<uint16_t>();
    pairing = stream.readIntBE<uint16_t>();
    flags = stream.readIntBE<uint16_t>();
    entrypoint = stream.readIntBE<uint32_t>();
    length = stream.readIntBE<uint32_t>();
    stream.read(salt, 0x10);

    data = new uint8_t[length - 0x20];
    stream.read(data, length - 0x20);
}

char *Bootloader::getMagic() {
    magicStr[0] = static_cast<char>((magic >> 8) & 0xFF);
    magicStr[1] = static_cast<char>(magic & 0xFF);
    magicStr[2] = 0x00;

    return magicStr;
}

void Bootloader::writeHeader(xe::io::Stream &stream) {
    stream.writeIntBE(magic);
    stream.writeIntBE(version);
    stream.writeIntBE(pairing);
    stream.writeIntBE(flags);
    stream.writeIntBE(entrypoint);
    stream.writeIntBE(length);
    stream.write(salt, 0x10);
}

void Bootloader::writeDecrypted(xe::io::Stream &output, const uint8_t *key, uint8_t *outDigest) {
    writeHeader(output);

    // HMAC-SHA the salt in the header, using the input key (from the previous bootloader or CPU ROM key)
    EXCRYPT_HMACSHA_STATE state{ 0 };
    uint8_t digest[0x10]{ 0 };

    ExCryptHmacShaInit(&state, key, 0x10);
    ExCryptHmacShaUpdate(&state, salt, 0x10);
    ExCryptHmacShaFinal(&state, digest, 0x10);

    // RC4 decrypt the body of the bootloader
    auto decrypted = new uint8_t[length - 0x20];
    memcpy(decrypted, &data[0], length - 0x20);
    ExCryptRc4(digest, 0x10, &decrypted[0], length - 0x20);

    output.write(decrypted, length - 0x20);
    delete[](decrypted);

    // Copy out the digest for the next bootloader, if requested
    if(outDigest != nullptr) {
        memcpy(outDigest, digest, 0x10);
    }
}

void Bootloader::writeKernel(xe::io::Stream &output, const uint8_t *key) {
    xe::io::MemoryStream decrypted;
    writeDecrypted(decrypted, key);

    decrypted.seek(0x30, std::ios::beg);

    size_t totalCompressedSize = 0, totalUncompressedSize = 0;
    int num = this->length - 0x30;

    int i = 0;

    while(decrypted.position() < decrypted.length()) {
        auto compressedSize = decrypted.readIntBE<uint16_t>();
        auto uncompressedSize = decrypted.readIntBE<uint16_t>();

        num -= 4;

        totalCompressedSize += compressedSize;
        totalUncompressedSize += uncompressedSize;

        decrypted.seek(compressedSize, std::ios::cur);

        num -= compressedSize;

        std::cout << std::hex << decrypted.position() << std::endl;

        if(uncompressedSize != 0x8000) {
            break;
        }

        i++;
    }

    auto src = new uint8_t[totalCompressedSize];
    auto dst = new uint8_t[totalUncompressedSize];
    auto dst_p = dst;

    decrypted.seek(0x30, std::ios::beg);

    auto bytesRead = 0;

    xlzx_params lzxParams{ 0 };
    lzxParams.window_size = XLZX_WINDOW_SIZE_DEFAULT;

    auto decoder = xlzx_decoder_create(&lzxParams);

    while(true) {
        auto compressedSize = decrypted.readIntBE<uint16_t>();
        auto uncompressedSize = decrypted.readIntBE<uint16_t>();
        size_t actualUncompressedSize = 0;

        decrypted.read(&src[bytesRead], compressedSize);

        auto lzxResult = xlzx_decoder_decompress(decoder, &src[bytesRead], compressedSize, dst_p, uncompressedSize, &actualUncompressedSize);

        assert(lzxResult == XLZX_OK);

        output.write(dst_p, uncompressedSize);
        dst_p += uncompressedSize;

        bytesRead += compressedSize;
        if(bytesRead >= totalCompressedSize) break;
    }


    return;
}

void Bootloader::write(xe::io::Stream &output) {
    writeHeader(output);
    output.write(data, length - 0x20);
}

size_t Bootloader::getAlignedSize() {
    return ((length + 0xf) &  ~0xf);
}

};
};