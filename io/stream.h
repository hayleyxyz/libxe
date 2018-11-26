//
// Created by yuikonnu on 30/10/2018.
//

#ifndef LIBXENON_STREAM_H
#define LIBXENON_STREAM_H

#include "../types.h"
#include "../endian.h"

namespace xe {
namespace io {

class Stream {
public:
    enum seekdir {beg, cur, end};

    virtual ~Stream() { };

    virtual bool isOpen() = 0;
    virtual size_t length() = 0;
    virtual off_t position() = 0;
    virtual size_t read(uint8_t *buf, size_t length) = 0;
    virtual size_t write(uint8_t *buf, size_t length) = 0;
    virtual off_t seek(off_t pos, seekdir dir) = 0;

    off_t seek(off_t pos) {
        return seek(pos, seekdir::beg);
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



#endif //LIBXENON_STREAM_H
