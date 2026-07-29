//
// Created by github.com/hayleyxyz on 29/10/2018.
//

#pragma once

#include <fstream>
#include "../types.h"
#include "stream.h"

namespace xe {
namespace io {

class FileStream : public Stream {
public:
    FileStream();
    ~FileStream();


    void open(const char *path, std::ios_base::openmode mode);
    void open(std::string &path, std::ios_base::openmode mode) {
        open(path.c_str(), mode);
    };

    void close();
    bool isOpen();
    size_t length();
    size_t position();
    size_t seek(size_t pos, std::ios_base::seekdir dir);
    size_t read(uint8_t *buf, size_t length);
    size_t write(uint8_t *buf, size_t length);


protected:
    std::fstream stream;
};

};
};
