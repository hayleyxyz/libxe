//
// Created by yuikonnu on 29/10/2018.
//

#include "bootloader.h"
#include "../io/memory_stream.h"
#include <cryptopp/hmac.h>
#include <cryptopp/sha.h>
#include <cryptopp/arc4.h>

#include "../compression/lzx.c"

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
    CryptoPP::HMAC<CryptoPP::SHA1> hmac(key, 0x10);
    uint8_t digest[0x10];
    hmac.Update(salt, 0x10);
    hmac.TruncatedFinal(digest, 0x10);

    // Resulting hash is used for decryption
    CryptoPP::ARC4 arc4;
    arc4.SetKey(digest, 0x10);

    // RC4 decrypt the body of the bootloader
    auto decrypted = new uint8_t[length - 0x20];
    arc4.ProcessString(&decrypted[0], &data[0], length - 0x20);
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

    decrypted.seek(0x30, xe::io::Stream::beg);

    size_t totalCompressedSize = 0, totalUncompressedSize = 0;
    int i = 1;
    int num = 78;

    while(--num > 0) {
        auto compressedSize = decrypted.readIntBE<uint16_t>();
        auto uncompressedSize = decrypted.readIntBE<uint16_t>();

        totalCompressedSize += compressedSize;
        totalUncompressedSize += uncompressedSize;

        decrypted.seek(compressedSize, xe::io::Stream::cur);

        std::cout << decrypted.position() << std::endl;

        if(uncompressedSize != 0x8000) {
            break;
        }

        i++;
    }

    auto src = new uint8_t[totalCompressedSize];
    auto dst = new uint8_t[totalUncompressedSize];
    auto dst_p = dst;

    decrypted.seek(0x30, xe::io::Stream::beg);

    auto bytesRead = 0;

    LZXinit(15);

    while(true) {
        auto compressedSize = decrypted.readIntBE<uint16_t>();
        auto uncompressedSize = decrypted.readIntBE<uint16_t>();

        decrypted.read(&src[bytesRead], compressedSize);

        LZXdecompress(&src[bytesRead], dst_p, compressedSize, uncompressedSize);

        //auto r = lzx_decompressor_ops.decompress(&src[bytesRead], compressedSize, dst_p, uncompressedSize, ptr);
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