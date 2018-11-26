//
// Created by yuikonnu on 30/10/2018.
//

#ifndef LIBXENON_SFCX_FILE_STREAM_H
#define LIBXENON_SFCX_FILE_STREAM_H

#include "../io/file_stream.h"
#include "sfcx_image.h"

namespace xe {
namespace sfcx {

class LogicalStream : public xe::io::Stream {
public:
    LogicalStream(SfcxImage &image);
    bool isOpen();
    size_t length();
    off_t position();
    size_t read(uint8_t *buf, size_t length);
    size_t write(uint8_t *buf, size_t length);
    off_t seek(off_t pos, seekdir dir);

protected:
    SfcxImage &image;
};

}
}

#endif //LIBXENON_SFCX_FILE_STREAM_H
