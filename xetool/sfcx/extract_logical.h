/*
* Created by yuikonnu on 31/10/2018.
*/

#ifndef LIBXE_EXTRACT_LOGICAL_H
#define LIBXE_EXTRACT_LOGICAL_H

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

#endif //LIBXE_EXTRACT_LOGICAL_H
