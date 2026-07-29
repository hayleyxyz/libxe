/*
* Created by github.com/hayleyxyz on 31/10/2018.
*/

#pragma once

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