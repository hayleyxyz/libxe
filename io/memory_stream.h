/*
* Created by yuikonnu on 02/11/2018.
*/

#ifndef LIBXE_MEMORY_STREAM_H
#define LIBXE_MEMORY_STREAM_H

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
    off_t position();
    off_t seek(off_t pos, seekdir dir);
    size_t read(uint8_t *buf, size_t length);
    size_t write(uint8_t *input, size_t length);

protected:
    uint8_t *buf = nullptr;
    size_t buflength = 0;
    off_t bufptr = 0;

    void allocateBuffer(size_t size);

    inline size_t align(size_t s) {
        auto mask = (4096ull)-1;
        return (((s)+(mask))&~(mask));
    }

};

};
};

#endif //LIBXE_MEMORY_STREAM_H
