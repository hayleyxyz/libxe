/*
* Created by yuikonnu on 31/10/2018.
*/

#include "application.h"

namespace xetool {

Module *Application::findModule(const char *moduleName) {
    for (auto &m : modules) {
        if (strcmp(m->name, moduleName) == 0) {
            return m;
        }
    }

    return nullptr;
}

};