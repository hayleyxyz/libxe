/*
* Created by github.com/hayleyxyz on 02/11/2018.
*/

#include "shadow_boot_rom.h"

namespace xe {
namespace rom {

void ShadowBootRom::read(xe::io::Stream &stream) {
    magic = stream.readIntBE<uint16_t>();
    version = stream.readIntBE<uint16_t>();

    stream.seek(0x08, std::ios::beg);
    bootloaderOffset = stream.readIntBE<uint32_t>();
    length = stream.readIntBE<uint32_t>();
    stream.read(reinterpret_cast<uint8_t *>(&copyright[0]), sizeof(copyright));

    stream.seek(0x60, std::ios::beg);
    kvOffset = stream.readIntBE<uint32_t>();

    stream.seek(0x78, std::ios::beg);
    smcSize = stream.readIntBE<uint32_t>();
    smcOffset = stream.readIntBE<uint32_t>();

    stream.seek(smcOffset, std::ios::beg);

    smc = new xe::bootloaders::SMC();
    smc->read(stream, smcSize);

    stream.seek(bootloaderOffset, std::ios::beg);

    for(int i = 0; i < 4; ++i) {
        auto bl = new xe::bootloaders::Bootloader();
        bl->read(stream);
        bootloaders.push_back(bl);
    }
}

};
};