/*
* Created by yuikonnu on 31/10/2018.
*/

#include "module.h"

namespace xetool {

Command *Module::findCommand(const char *commandName) {
    for (auto &c : commands) {
        if (strcmp(c->name, commandName) == 0) {
            return c;
        }
    }

    return nullptr;
}

};
