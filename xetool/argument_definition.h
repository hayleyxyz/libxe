/*
* Created by github.com/hayleyxyz on 31/10/2018.
*/

#pragma once

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
