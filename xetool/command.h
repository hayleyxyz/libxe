/*
* Created by yuikonnu on 31/10/2018.
*/

#ifndef LIBXE_COMMAND_H
#define LIBXE_COMMAND_H

#include <vector>
#include "argument_definition.h"
#include "command_line_input.h"

namespace xetool {

class Command {
public:
    Command(const char *name) : name(name) {};

    virtual int execute(CommandLineInput &input) = 0;
    std::string definition();

    const char *name;
    std::vector<ArgumentDefinition *> arguments;
};

};

#endif //LIBXE_COMMAND_H
