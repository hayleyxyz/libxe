/*
* Created by yuikonnu on 02/11/2018.
*/

#ifndef LIBXE_EXTRACT_H
#define LIBXE_EXTRACT_H

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

#endif //LIBXE_EXTRACT_H
