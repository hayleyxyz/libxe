//
// Created by github.com/hayleyxyz on 29/10/2018.
//

#pragma once

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
    size_t offset = -1;
    char magicStr[3];
};

};
};
