/*
* Created by yuikonnu on 02/11/2018.
*/

#ifndef LIBXE_SMC_H
#define LIBXE_SMC_H

#include "../io/stream.h"

namespace xe {
namespace bootloaders {

class SMC {
public:
    void read(xe::io::Stream &stream, size_t length);
    void writeDecrypted(xe::io::Stream &stream);
    void decrypt(uint8_t *output);

public:
    uint8_t *data = nullptr;
    size_t dataLength = -1;
};

};
};

#endif //LIBXE_SMC_H
