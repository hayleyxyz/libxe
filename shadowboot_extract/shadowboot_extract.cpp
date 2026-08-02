#include <cstdio>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include "../rom/shadowboot_rom.h"
#include "../crypto/keys.h"

bool writeBootloader(xe::bootloaders::Bootloader* bl, bool decrypted, const uint8_t* key, uint8_t* out_digest);
std::string getBlFileName(xe::bootloaders::Bootloader* bl, bool decrypted);

void printUsage(const char* prog)
{
    fprintf(stderr, "Usage: %s [options] <shadowboot.bin>\n", prog);
    fprintf(stderr, "Extracts bootloaders from shadowboot ROMs (xboxromw2d.bin, xboxrom_update.bin)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -d, --decrypt\n");
    fprintf(stderr, "    Write decrypted bootloaders (Default)\n\n");
    fprintf(stderr, "  -e, --encrypt\n");
    fprintf(stderr, "    Write encrypted bootloaders\n\n");
}

int main(const int argc, const char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    const char *inputFile = argv[1];

    std::ifstream stream(inputFile, std::ios::binary);

    if (!stream.is_open()) {
        fprintf(stderr, "Failed to open input file: %s\n", inputFile);
        return 1;
    }

    xe::rom::ShadowBootRom shadowBootRom;
    shadowBootRom.read(stream);

    constexpr auto smcPath = "SMC.dec.bin";
    std::ofstream smcStream(smcPath, std::ios::binary);
    if (!smcStream.is_open()) {
        std::cerr << "Failed to open " << smcPath << "for writing" << std::endl;
        return 1;
    }

    shadowBootRom.smc->writeDecrypted(smcStream);

    smcStream.close();

    std::ofstream bootloaderStream;

    auto sb = shadowBootRom.bootloaders.at(0);
    writeBootloader(sb, true, reinterpret_cast<const uint8_t*>(xe::crypto::keys::cpuRomKey), nullptr);

    uint8_t blkey[0x10]{ 0 };

    auto sc = shadowBootRom.bootloaders.at(1);
    writeBootloader(sc, true, blkey, blkey);

    auto sd = shadowBootRom.bootloaders.at(2);
    writeBootloader(sd, true, blkey, blkey);

    auto se = shadowBootRom.bootloaders.at(3);
    writeBootloader(se, true, blkey, nullptr);


    std::stringstream kernelss;
    kernelss << "xboxkrnl." << se->version << ".exe";

    bootloaderStream.open(kernelss.str(), std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    if (!bootloaderStream.is_open()) {
        std::cerr << "Failed to open kernel output: " << kernelss.str() << std::endl;
        return 1;
    }

    std::cout << kernelss.str() << std::endl;

    se->write_kernel(bootloaderStream, blkey);

    return 0;
}

bool writeBootloader(xe::bootloaders::Bootloader* bl, bool decrypted, const uint8_t *key, uint8_t *out_digest) {
    auto blpath = getBlFileName(bl, decrypted);

    std::ofstream stream(blpath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        std::cerr << "Failed to open bootloader output: " << blpath << std::endl;
        return false;
    }

    std::cout << blpath << std::endl;

    bl->write_decrypted(stream, key, out_digest);

    return true;
}

std::string getBlFileName(xe::bootloaders::Bootloader* bl, bool decrypted) {
    std::stringstream ss;
    ss << bl->get_magic() << "." << bl->version << "." << (decrypted ? "decrypted" : "encrypted") << ".bin";
    return ss.str();
}