/*
* Created by yuikonnu on 02/11/2018.
*/

#ifndef LIBXE_SHADOWBOOT_LIST_H
#define LIBXE_SHADOWBOOT_LIST_H

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


#endif //LIBXE_SHADOWBOOT_LIST_H
