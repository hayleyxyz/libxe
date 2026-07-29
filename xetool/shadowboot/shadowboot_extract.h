/*
* Created by github.com/hayleyxyz on 02/11/2018.
*/

#pragma once

#include <xetool/command.h>
#include <bootloaders/bootloader.h>
#include <io/file_stream.h>

namespace xetool {
namespace shadowboot {

class Extract : public Command {
public:
    Extract();
    int execute(CommandLineInput &input) override;

protected:
    std::string getBootloaderPath(xe::bootloaders::Bootloader *bl, std::string &outputDir, bool decrypted);
    bool openBootloaderStream(xe::io::FileStream &stream, std::string &outputDir, xe::bootloaders::Bootloader *bl, bool decrypted);
};

};
};
