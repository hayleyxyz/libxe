/*
* Created by github.com/hayleyxyz on 02/11/2018.
*/

#pragma once

#include "stream.h"

namespace xe {
namespace io {

class MemoryStream : public Stream {
public:
    MemoryStream() : MemoryStream(4096) { };
    MemoryStream(size_t initialSize);

    void close();
    bool isOpen();
    size_t length();
    size_t position();
    size_t seek(size_t pos, std::ios_base::seekdir dir);
    size_t read(uint8_t *buf, size_t length);
    size_t write(uint8_t *input, size_t length);

protected:
    uint8_t *buf = nullptr;
    size_t buflength = 0;
    size_t bufptr = 0;

    void allocateBuffer(size_t size);

    inline size_t align(size_t s) {
        auto mask = (4096ull)-1;
        return (((s)+(mask))&~(mask));
    }

};

};
};
