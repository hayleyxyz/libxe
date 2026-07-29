/*
* Created by github.com/hayleyxyz on 31/10/2018.
*/

#pragma once

#include <vector>
#include "module.h"

namespace xetool {

class Application {
public:
    std::vector<Module *> modules;

    Module *findModule(const char *moduleName);
};

}
