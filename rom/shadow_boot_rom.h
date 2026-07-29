/*
* Created by github.com/hayleyxyz on 02/11/2018.
*/

#pragma once

#include "../io/stream.h"
#include "../bootloaders/smc.h"
#include "../bootloaders/bootloader.h"
#include <vector>

namespace xe {
namespace rom {

class ShadowBootRom {
public:
    void read(xe::io::Stream &stream);

    xe::bootloaders::SMC *smc;
    std::vector<xe::bootloaders::Bootloader *> bootloaders;

public:
    uint16_t magic; // 0x00
    uint16_t version; // 0x02
    uint32_t bootloaderOffset; // 0x08
    uint32_t length; // 0x0c
    char copyright[0x50]; // 0x10-0x46
    uint32_t kvOffset; // 0x60;
    uint32_t smcSize; // 0x78
    uint32_t smcOffset; // 0x7d
};

};
};
