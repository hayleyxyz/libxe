//
// Created by yuikonnu on 30/10/2018.
//

#include "logical_stream.h"

namespace xe {
namespace sfcx {

LogicalStream::LogicalStream(xe::sfcx::SfcxImage &image) : image(image) {

}

bool LogicalStream::isOpen() {
    return image.innerStream().isOpen();
}

size_t LogicalStream::length() {
    return image.logicalLength();
}

off_t LogicalStream::position() {
    return image.physicalToLogicalAddress(image.innerStream().position());
}

off_t LogicalStream::seek(off_t pos, io::Stream::seekdir dir) {
    off_t newpos = 0;

    switch(dir) {
        case beg:
            newpos = pos;
            break;

        case cur:
            newpos = position() + pos;
            break;

        case end:
            newpos = length() + pos;
            break;
    }

    return image.innerStream().seek(
            image.logicalToPhysicalAddress(newpos),
            io::Stream::beg
    );
}

size_t LogicalStream::read(uint8_t *buf, size_t length) {
    auto bytesRead = 0;
    uint8_t *bufptr = buf;

    while(bytesRead < length) {
        auto physicalAddress = image.innerStream().position();
        auto bytesRemainingInPhysicalBlock = image.pageTotalSize - (physicalAddress % image.pageTotalSize);

        if(bytesRemainingInPhysicalBlock <= image.pageMetaSize) {
            // We're up against, or inside the "meta" section, so skip past it
            image.innerStream().seek(bytesRemainingInPhysicalBlock, xe::io::Stream::cur);
        }

        // Read each logical block at a time
        auto bytesRemainingInLogicalBlock = image.pageDataSize - (position() % image.pageDataSize);
        auto bytesToRead = (length - bytesRead);

        if(bytesRemainingInLogicalBlock < bytesToRead) {
            bytesToRead = bytesRemainingInLogicalBlock;
        }

        auto justRead = image.innerStream().read(bufptr, bytesToRead);

        bufptr += justRead;
        bytesRead += justRead;

        if(justRead != bytesToRead) break;
    }

    return static_cast<size_t>(bytesRead);
}

size_t LogicalStream::write(uint8_t *buf, size_t length) {
    throw std::runtime_error("unimplemented");
}

}
}