//
// Created by github.com/hayleyxyz on 30/10/2018.
//

#pragma once

#include "../io/file_stream.h"
#include "sfcx_image.h"

namespace xe {
namespace sfcx {

class LogicalStream : public xe::io::Stream {
public:
    LogicalStream(SfcxImage &image);
    bool isOpen();
    size_t length();
    size_t position();
    size_t read(uint8_t *buf, size_t length);
    size_t write(uint8_t *buf, size_t length);
    size_t seek(size_t pos, std::ios_base::seekdir dir);

protected:
    SfcxImage &image;
};

}
}
