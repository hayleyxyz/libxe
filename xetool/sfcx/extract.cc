/*
* Created by yuikonnu on 02/11/2018.
*/

#include <iostream>
#include <sstream>
#include "extract.h"
#include "../../io/io.h"
#include "../../sfcx/sfcx_image.h"
#include "../../crypto/keys.h"

namespace xetool {
namespace sfcx {

Extract::Extract() : Command("extract") {
    arguments.push_back(new ArgumentDefinition("input", ArgumentDefinition::required));
    arguments.push_back(new ArgumentDefinition("output-dir", ArgumentDefinition::required));
}

int Extract::execute(CommandLineInput &input) {
    std::string inputPath;
    std::string outputPath;

    input.getArgumentValue("input", inputPath);
    input.getArgumentValue("output-dir", outputPath);

    if (!xe::io::isFileReadable(inputPath)) {
        std::cerr << "Input file not readable: " << inputPath << std::endl;
        return 1;
    }

    if (!xe::io::isDirectory(outputPath)) {
        std::cerr << "Output directory does not exist: " << outputPath << std::endl;
        return 1;
    }

    xe::io::FileStream nand;
    nand.open(inputPath, std::ios_base::in | std::ios_base::binary);
    if (!nand.isOpen()) return 0;

    xe::sfcx::SfcxImage image(nand);

    xe::io::FileStream bootloaderStream;

    auto sb = image.bootloaders.at(0);
    if(!openBootloaderStream(bootloaderStream, outputPath, sb)) return 1;
    sb->writeDecrypted(bootloaderStream, xe::crypto::keys::cpuRomKey);
    bootloaderStream.close();

    uint8_t blkey[0x10] {0};

    auto sc = image.bootloaders.at(1);
    if(!openBootloaderStream(bootloaderStream, outputPath, sc)) return 1;
    sc->writeDecrypted(bootloaderStream, blkey, blkey);
    bootloaderStream.close();

    auto sd = image.bootloaders.at(2);
    if(!openBootloaderStream(bootloaderStream, outputPath, sd)) return 1;
    sd->writeDecrypted(bootloaderStream, blkey, blkey);
    bootloaderStream.close();

    auto se = image.bootloaders.at(3);
    if(!openBootloaderStream(bootloaderStream, outputPath, se)) return 1;
    se->writeDecrypted(bootloaderStream, blkey);
    bootloaderStream.close();

    return 0;
}

bool Extract::openBootloaderStream(xe::io::FileStream &stream, std::string &outputDir, xe::bootloaders::Bootloader *bl) {
    auto blpath = getBootloaderPath(bl, outputDir);

    stream.open(blpath, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    if(!stream.isOpen()) {
        std::cerr << "Failed to open bootloader output: " << blpath << std::endl;
        return false;
    }

    std::cout << blpath << std::endl;

    return true;
}

std::string Extract::getBootloaderPath(xe::bootloaders::Bootloader *bl, std::string &outputDir) {
    auto sep = outputDir.substr(outputDir.length() - 1, 1) == "/" ? "" : "/";

    std::stringstream ss;
    ss << outputDir << sep << bl->getMagic() << "." << bl->version << ".dec.bin";
    return ss.str();
}

};
};