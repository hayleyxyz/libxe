/*
* Created by github.com/hayleyxyz on 02/11/2018.
*/

#pragma once

#include "../command.h"

namespace xetool {
namespace shadowboot {

class List : public Command {
public:
    List();

    int execute(CommandLineInput &input) override;
};

};
};
