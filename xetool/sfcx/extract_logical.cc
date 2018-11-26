/*
* Created by yuikonnu on 31/10/2018.
*/

#include <iostream>
#include <io/io.h>
#include <io/file_stream.h>
#include <sfcx/sfcx_image.h>
#include <sfcx/logical_stream.h>
#include "extract_logical.h"

namespace xetool {
namespace sfcx {

ExtractLogical::ExtractLogical() : Command("extract-logical") {
    arguments.push_back(new ArgumentDefinition("input", ArgumentDefinition::required));
    arguments.push_back(new ArgumentDefinition("output", ArgumentDefinition::required));
}

int ExtractLogical::execute(CommandLineInput &input) {
    std::string inputPath;
    std::string outputPath;

    input.getArgumentValue("input", inputPath);
    input.getArgumentValue("output", outputPath);

    if (!xe::io::isFileReadable(inputPath)) {
        std::cerr << "Input file not readable: " << inputPath << std::endl;
        return 1;
    }

    xe::io::FileStream nand;
    nand.open(inputPath, std::ios_base::in | std::ios_base::binary);
    if (!nand.isOpen()) return 0;

    xe::sfcx::SfcxImage image(nand);

    xe::sfcx::LogicalStream lstream(image);


    lstream.seek(0, xe::io::Stream::seekdir::beg);

    uint8_t *buf = new uint8_t[lstream.length()];
    lstream.seek(0, xe::sfcx::LogicalStream::beg);
    lstream.read(buf, lstream.length());

    xe::io::writeAllBytes(outputPath, buf, lstream.length());

    return 0;
}

};
};
