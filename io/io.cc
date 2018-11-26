//
// Created by yuikonnu on 29/10/2018.
//

#include "io.h"
#include "file_stream.h"
#include <sys/types.h>
#include <sys/stat.h>

namespace xe {
namespace io {

bool writeAllBytes(std::string &path, uint8_t *buffer, size_t length) {
    FileStream fs;
    fs.open(path, FileStream::out);
    if(!fs.isOpen()) return false;

    fs.write(buffer, length);
    fs.close();

    return true;
}

bool isFileReadable(std::string &path) {
    std::ifstream infile(path);
    return infile.good();
}

bool isFileWriteable(std::string &path) {
    std::ofstream infile(path);
    return infile.good();
}

bool isDirectory(std::string &path) {
    struct stat info { 0 };

    if(stat(path.c_str(), &info) != 0) return false;

    return (info.st_mode & S_IFDIR) > 0;
}

};
};