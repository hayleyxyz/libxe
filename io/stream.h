//
// Created by github.com/hayleyxyz on 30/10/2018.
//

#pragma once

#include "../types.h"
#include "../endian.h"
#include <iostream>

namespace xe {
namespace io {

class Stream {
public:
    virtual ~Stream() { };

    virtual bool isOpen() = 0;
    virtual size_t length() = 0;
    virtual size_t position() = 0;
    virtual size_t read(uint8_t *buf, size_t length) = 0;
    virtual size_t write(uint8_t *buf, size_t length) = 0;
    virtual size_t seek(size_t pos, std::ios_base::seekdir dir) = 0;

    size_t seek(size_t pos) {
        return seek(pos, std::ios::beg);
    };

    template <typename T>
    T readInt() {
        T result;
        read(reinterpret_cast<uint8_t *>(&result), sizeof(T));
        return result;
    };

    template <typename T>
    T readIntBE() {
        T result = readInt<T>();
        return xe::endian::getBigEndianToNative<T>(result);
    };

    template <typename T>
    void writeInt(T value) {
        write(reinterpret_cast<uint8_t *>(&value), sizeof(T));
    };

    template <typename T>
    void writeIntBE(T value) {
        writeInt(xe::endian::getBigEndianToNative<T>(value));
    };
};

};
};
