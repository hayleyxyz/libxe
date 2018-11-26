//
// Created by yuikonnu on 29/10/2018.
//

#ifndef LIBXENON_FILE_H
#define LIBXENON_FILE_H

#include "../types.h"
#include <string>

namespace xe {
namespace io {

bool writeAllBytes(std::string &path, uint8_t *buffer, size_t length);
bool isFileReadable(std::string &path);
bool isFileWriteable(std::string &path);
bool isDirectory(std::string &path);

};
};

#endif //LIBXENON_FILE_H
