//
// Created by yuikonnu on 29/10/2018.
//

#ifndef LIBXENON_FILE_STREAM_H
#define LIBXENON_FILE_STREAM_H

#include <fstream>
#include "../types.h"
#include "stream.h"

namespace xe {
namespace io {

class FileStream : public Stream {
public:
    FileStream();
    ~FileStream();

    typedef unsigned int openmode;
    static const openmode app    = 0x01;
    static const openmode ate    = 0x02;
    static const openmode binary = 0x04;
    static const openmode in     = 0x08;
    static const openmode out    = 0x10;
    static const openmode trunc  = 0x20;


    void open(const char *path, openmode mode);
    void open(std::string &path, openmode mode) {
        open(path.c_str(), mode);
    };

    void close();
    bool isOpen();
    size_t length();
    off_t position();
    off_t seek(off_t pos, seekdir dir);
    size_t read(uint8_t *buf, size_t length);
    size_t write(uint8_t *buf, size_t length);


protected:
    std::fstream stream;
};

};
};

#endif //LIBXENON_FILE_STREAM_H
