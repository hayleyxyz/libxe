//
// Created by yuikonnu on 30/10/2018.
//

#ifndef LIBXENON_PAGE_H
#define LIBXENON_PAGE_H

#include "../io/file_stream.h"
#include "../types.h"

namespace xe {
namespace sfcx {

class Page {

public:
    Page(uint16_t pageNumber) : pageNumber(pageNumber) { };
    void read(xe::io::FileStream &stream);

    uint16_t pageNumber;
    uint16_t blockNumber = 0;
    bool isBad = false;
    bool isEmpty = false;
};

}
}

#endif //LIBXENON_PAGE_HP