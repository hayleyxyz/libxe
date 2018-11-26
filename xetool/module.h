/*
* Created by yuikonnu on 31/10/2018.
*/

#ifndef LIBXE_MODULE_H
#define LIBXE_MODULE_H

#include <string>
#include <vector>
#include "command.h"

namespace xetool {

class Module {
public:
    Module(const char *name) : name(name) {};

    const char *name;
    std::vector<Command *> commands;

    Command *findCommand(const char *commandName);
};

};

#endif //LIBXE_MODULE_HM