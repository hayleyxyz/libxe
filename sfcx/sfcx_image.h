//
// Created by yuikonnu on 29/10/2018.
//

#ifndef LIBXENON_SFCX_IMAGE_H
#define LIBXENON_SFCX_IMAGE_H

#include "../types.h"
#include "../io/file_stream.h"
#include "../bootloaders/bootloader.h"
#include "page.h"
#include <map>
#include <vector>

namespace xe {
namespace sfcx {

class SfcxImage {
public:
    SfcxImage(xe::io::FileStream &stream);
    const int pageDataSize = 0x200;
    const int pageMetaSize = 0x10;
    const int pageTotalSize = pageDataSize + pageMetaSize;
    const int pagesPerBlock = 32;
    int reservedBlockCount = 32;
    const int blockTotalSize = pageTotalSize * pagesPerBlock;

    int32_t logicalToPhysicalAddress(int32_t logical);
    int32_t physicalToLogicalAddress(int32_t physical);
    size_t logicalLength();
    size_t physicalLength();

    bool isValid() {
        return valid;
    };

    xe::io::FileStream &innerStream() {
        return stream;
    };

    std::vector<xe::bootloaders::Bootloader *> bootloaders;

protected:
    bool valid = false;
    bool detectImageGeometry();
    void read();
    void readLogicalMap();
    void dumpBlockTable();
    xe::io::FileStream &stream;
    std::map<int, int> logicalMap;

    void readBootLoaders();
};

}
}

#endif //LIBXENON_SFCX_IMAGE_H
