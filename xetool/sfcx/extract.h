/*
* Created by yuikonnu on 02/11/2018.
*/

#ifndef LIBXE_SFCX_EXTRACT_H
#define LIBXE_SFCX_EXTRACT_H


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


#endif //LIBXE_SFCX_EXTRACT_H
