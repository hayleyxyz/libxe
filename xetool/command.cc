/*
* Created by yuikonnu on 02/11/2018.
*/

#include <iostream>
#include <string>
#include <sstream>
#include "command.h"

namespace xetool {

std::string Command::definition() {
    std::stringstream ss;
    ss << name;

    for(auto &a : arguments) {
        ss << " ";
        if(a->mode == ArgumentDefinition::optional) {
            ss << "[" << a->name << "]";
        }
        else {
            ss << a->name;
        }
    }

    return ss.str();
}

};