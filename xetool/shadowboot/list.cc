/*
* Created by yuikonnu on 02/11/2018.
*/

#include "list.h"

#include <iostream>
#include <io/io.h>
#include <io/file_stream.h>
#include <rom/shadow_boot_rom.h>

namespace xetool {
namespace shadowboot {

List::List() : Command("list") {
    arguments.push_back(new ArgumentDefinition("input", ArgumentDefinition::required));
}

int List::execute(CommandLineInput &input) {
    std::string inputPath;

    input.getArgumentValue("input", inputPath);

    if (!xe::io::isFileReadable(inputPath)) {
        std::cerr << "Input file not readable: " << inputPath << std::endl;
        return 1;
    }

    xe::io::FileStream fs;
    fs.open(inputPath, std::ios_base::in | std::ios_base::binary);
    if (!fs.isOpen()) return 0;

    xe::rom::ShadowBootRom rom;
    rom.read(fs);

    std::cout << "ROM version: " << rom.version << std::endl;
    std::cout << "Bootloaders:" << std::endl;

    int i = 0;
    for (auto &bl : rom.bootloaders) {
        std::cout << std::endl;
        std::cout << "Bootloader " << (++i) << ":" << std::endl;
        std::cout << "\tMagic: " << bl->getMagic() << std::endl;
        std::cout << "\tVersion: " << bl->version << std::endl;
        std::cout << "\tEntrypoint: 0x" << std::hex << bl->entrypoint << std::dec << std::endl;

    }

    return 0;
}

};
};