/*
* Created by github.com/hayleyxyz on 31/10/2018.
*/

#pragma once

#include "../command.h"

namespace xetool {
namespace sfcx {

class ExtractLogical : public Command {
public:
    ExtractLogical();

    int execute(CommandLineInput &input) override;
};

};
};
