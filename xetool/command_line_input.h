/*
* Created by yuikonnu on 31/10/2018.
*/

#ifndef LIBXE_COMMANDLINEINPUT_H
#define LIBXE_COMMANDLINEINPUT_H

#include <map>
#include <vector>
#include <string>
#include "argument_definition.h"

namespace xetool {

class Command;

class CommandLineInput {
public:
    void parse(Command *command, int argc, const char **argv);

    bool validate(std::vector<std::string> &messages);

    bool getArgumentValue(const char *name, std::string &value);

    std::map<std::string, std::string> arguments;

protected:
    Command *command = nullptr;

    ArgumentDefinition *getNextUnfilledArgument();
};

};

#endif //LIBXE_COMMANDLINEINPUT_H
