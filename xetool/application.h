/*
* Created by yuikonnu on 31/10/2018.
*/

#ifndef LIBXE_APPLICATION_H
#define LIBXE_APPLICATION_H

#include <vector>
#include "module.h"

namespace xetool {

class Application {
public:
    std::vector<Module *> modules;

    Module *findModule(const char *moduleName);
};

}

#endif //LIBXE_APPLICATION_H
