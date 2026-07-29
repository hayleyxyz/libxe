/*
* Created by github.com/hayleyxyz on 02/11/2018.
*/

#pragma once

#include <xetool/command.h>
#include "../../io/file_stream.h"
#include "../../bootloaders/bootloader.h"

namespace xetool {
namespace sfcx {

class Extract : public Command {
public:
    Extract();
    int execute(CommandLineInput &input) override;

protected:
    bool openBootloaderStream(xe::io::FileStream &stream, std::string &outputDir, xe::bootloaders::Bootloader *bl);
    std::string getBootloaderPath(xe::bootloaders::Bootloader *bl, std::string &outputDir);
};

};
};
