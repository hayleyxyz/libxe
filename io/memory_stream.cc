/*
* Created by yuikonnu on 02/11/2018.
*/

#include <cstring>
#include <cassert>
#include "memory_stream.h"

xe::io::MemoryStream::MemoryStream(size_t initialSize) {
    allocateBuffer(initialSize);
}

void xe::io::MemoryStream::allocateBuffer(size_t size) {
    if(size <= buflength) return;

    auto temp = new uint8_t[align(size)];

    if(this->buf) {
        if(this->buflength > 0) {
            memcpy(temp, buf, buflength);
        }

        delete[] buf;
    }

    buf = temp;
    buflength = size;
}

bool xe::io::MemoryStream::isOpen() {
    return (this->buf != nullptr);
}

size_t xe::io::MemoryStream::length() {
    return buflength;
}

off_t xe::io::MemoryStream::position() {
    return bufptr;
}

off_t xe::io::MemoryStream::seek(off_t pos, xe::io::Stream::seekdir dir) {
    switch(dir) {
        case beg:
            assert(pos < buflength);
            bufptr = pos;
            break;
        case cur:
            assert(bufptr + pos < buflength);
            bufptr += pos;
            break;
        case end:
            assert(buflength + pos < buflength);
            bufptr = buflength + pos;
            break;
        default:
            return -1;
    }

    return bufptr;
}

size_t xe::io::MemoryStream::read(uint8_t *output, size_t length) {
    if(bufptr + length >= buflength) {
        length = (buflength - bufptr);
    }

    memcpy(output, &this->buf[bufptr], length);
    bufptr += length;

    return length;
}

size_t xe::io::MemoryStream::write(uint8_t *input, size_t length) {
    allocateBuffer(bufptr + length);

    memcpy(&this->buf[bufptr], input, length);
    bufptr += length;

    return length;
}
