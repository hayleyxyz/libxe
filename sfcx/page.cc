//
// Created by yuikonnu on 30/10/2018.
//

#include "page.h"

namespace xe {
namespace sfcx {

void Page::read(xe::io::FileStream &stream) {
    uint8_t meta[0x10];

    stream.read(meta, 0x10);

    blockNumber = (meta[2] << 8 | meta[1]);

    isBad = !(meta[5] == 0xff);
    bool empty = true;

    for (int i = 0; i < 0x10; ++i) {
        if(meta[i] != 0xff) {
            empty = false;
            break;
        }
    }

    isEmpty = empty;
}

};
};
