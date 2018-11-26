//
// Created by yuikonnu on 29/10/2018.
//

#ifndef LIBXENON_BOOTLOADER_H
#define LIBXENON_BOOTLOADER_H

#include <cstdint>
#include <string>
#include "../io/stream.h"

namespace xe {
namespace bootloaders {

class Bootloader {
public:
    void read(xe::io::Stream &stream);
    void writeHeader(xe::io::Stream &stream);
    void writeDecrypted(xe::io::Stream &output, const uint8_t *key, uint8_t *digest = nullptr);
    void write(xe::io::Stream &output);
    void writeKernel(xe::io::Stream &output, const uint8_t *key);
    char *getMagic();
    size_t getAlignedSize();


public:
    // Structure
    uint16_t magic;
    uint16_t version;
    uint16_t pairing;
    uint16_t flags;
    uint32_t entrypoint;
    uint32_t length;
    uint8_t salt[0x10];

    uint8_t *data = nullptr;

protected:
    off_t offset = -1;
    char magicStr[3];
};

};
};

#endif //LIBXENON_BOOTLOADER_H
