/*
* Created by yuikonnu on 31/10/2018.
*/

#ifndef LIBXE_INPUT_ARGUMENT_H
#define LIBXE_INPUT_ARGUMENT_H

namespace xetool {

class ArgumentDefinition {
public:
    enum argmode {
        required, optional
    };

    ArgumentDefinition(const char *name, argmode mode) : name(name), mode(mode) {};
    const char *name;
    argmode mode;
};

};

#endif //LIBXE_INPUT_ARGUMENT_H
