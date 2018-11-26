/*
* Created by yuikonnu on 31/10/2018.
*/

#include <string>
#include <sstream>
#include "command_line_input.h"
#include "command.h"

namespace xetool {

void CommandLineInput::parse(Command *command, int argc, const char **argv) {
    this->command = command;

    for (int i = 0; i < argc; ++i) {
        std::string arg(argv[i]);

        if (arg.find("--") == 0 && arg.length() > 2) {
            // parse long arg
        }
        else if (arg.find("-") == 0 && arg.length() > 1) {
            // parse short arg
        }
        else {
            auto unfilled = getNextUnfilledArgument();
            if (unfilled == nullptr) {
                // TODO: handle unexpected args
                continue;
            }

            arguments.insert(std::make_pair(unfilled->name, arg));
        }
    }
}

ArgumentDefinition *CommandLineInput::getNextUnfilledArgument() {
    for (auto &a : command->arguments) {
        if (arguments.find(a->name) == arguments.end()) {
            return a;
        }
    }

    return nullptr;
}

bool CommandLineInput::validate(std::vector<std::string> &messages) {
    bool result = true;

    for (auto &a : command->arguments) {
        if (arguments.find(a->name) == arguments.end() && a->mode == a->required) {
            std::ostringstream err;
            err << "Argument \"" << a->name << "\" is required";
            messages.push_back(err.str());
            result = false;
        }
    }

    return result;
}

bool CommandLineInput::getArgumentValue(const char *name, std::string &value) {
    auto found = arguments.find(name);
    if (found == arguments.end()) {
        return false;
    }

    value = found->second;

    return true;
}

};