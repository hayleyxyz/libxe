/*
* Created by yuikonnu on 02/11/2018.
*/

#include "extract.h"
#include <io/io.h>
#include <io/file_stream.h>
#include <iostream>
#include <rom/shadow_boot_rom.h>
#include <crypto/keys.h>
#include <sstream>
#include <crypto/hmac_sha.h>

namespace xetool {
namespace shadowboot {

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

    if(!xe::io::isDirectory(outputPath)) {
        std::cerr << "Output directory does not exist: " << outputPath << std::endl;
        return 1;
    }

    xe::io::FileStream fs;
    fs.open(inputPath, std::ios_base::in | std::ios_base::binary);
    if (!fs.isOpen()) return 0;

    xe::rom::ShadowBootRom rom;
    rom.read(fs);

    auto smcPath = outputPath + "//" + "SMC.dec.bin";

    xe::io::FileStream smcStream;
    smcStream.open(smcPath, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    if(!smcStream.isOpen()) {
        std::cerr << "Failed to open smc output: " << smcPath << std::endl;
        return 1;
    }

    rom.smc->writeDecrypted(smcStream);
    smcStream.close();


    xe::io::FileStream bootloaderStream;

    auto sb = rom.bootloaders.at(0);
    if(!openBootloaderStream(bootloaderStream, outputPath, sb, true)) return 1;
    sb->writeDecrypted(bootloaderStream, xe::crypto::keys::cpuRomKey);
    bootloaderStream.close();

    if(!openBootloaderStream(bootloaderStream, outputPath, sb, false)) return 1;
    sb->write(bootloaderStream);
    bootloaderStream.close();

    uint8_t blkey[0x10] {0};

    auto sc = rom.bootloaders.at(1);
    if(!openBootloaderStream(bootloaderStream, outputPath, sc, true)) return 1;
    sc->writeDecrypted(bootloaderStream, blkey, blkey);
    bootloaderStream.close();

    if(!openBootloaderStream(bootloaderStream, outputPath, sc, false)) return 1;
    sc->write(bootloaderStream);
    bootloaderStream.close();

    auto sd = rom.bootloaders.at(2);
    if(!openBootloaderStream(bootloaderStream, outputPath, sd, true)) return 1;
    sd->writeDecrypted(bootloaderStream, blkey, blkey);
    bootloaderStream.close();

    if(!openBootloaderStream(bootloaderStream, outputPath, sd, false)) return 1;
    sd->write(bootloaderStream);
    bootloaderStream.close();

    auto se = rom.bootloaders.at(3);
    if(!openBootloaderStream(bootloaderStream, outputPath, se, true)) return 1;
    se->writeDecrypted(bootloaderStream, blkey);
    bootloaderStream.close();

    if(!openBootloaderStream(bootloaderStream, outputPath, se, false)) return 1;
    se->write(bootloaderStream);
    bootloaderStream.close();

#if 0
    auto kernelPath = outputPath + "/" + "xboxkrnl.exe";
    bootloaderStream.open(kernelPath, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    if(!bootloaderStream.isOpen()) {
        std::cerr << "Failed to open bootloader output: " << kernelPath << std::endl;
        return false;
    }

    se->writeKernel(bootloaderStream, blkey);

    bootloaderStream.close();
#endif

    return 0;
}


bool Extract::openBootloaderStream(xe::io::FileStream &stream, std::string &outputDir, xe::bootloaders::Bootloader *bl, bool decrypted) {
    auto blpath = getBootloaderPath(bl, outputDir, decrypted);

    stream.open(blpath, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    if(!stream.isOpen()) {
        std::cerr << "Failed to open bootloader output: " << blpath << std::endl;
        return false;
    }

    std::cout << blpath << std::endl;

    return true;
}

std::string Extract::getBootloaderPath(xe::bootloaders::Bootloader *bl, std::string &outputDir, bool decrypted) {
    auto sep = outputDir.substr(outputDir.length() - 1, 1) == "/" ? "" : "/";

    std::stringstream ss;
    ss << outputDir << sep << bl->getMagic() << "." << bl->version << "." << (decrypted ? "decrypted" : "encrypted") << ".bin";
    return ss.str();
}

};
};


