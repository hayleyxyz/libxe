/*
* Created by yuikonnu on 31/10/2018.
*/

#include <xetool/sfcx/extract_logical.h>
#include <iostream>
#include <xetool/shadowboot/list.h>
#include <xetool/shadowboot/extract.h>
#include <xetool/sfcx/extract.h>
#include "application.h"
#include "command_line_input.h"

xetool::Application *setupApplication() {
    auto app = new xetool::Application();

    auto sfcx = new xetool::Module("sfcx");
    sfcx->commands.push_back(new xetool::sfcx::ExtractLogical());
    sfcx->commands.push_back(new xetool::sfcx::Extract());
    app->modules.push_back(sfcx);

    auto shadowboot = new xetool::Module("shadowboot");
    shadowboot->commands.push_back(new xetool::shadowboot::List());
    shadowboot->commands.push_back(new xetool::shadowboot::Extract());
    app->modules.push_back(shadowboot);

    return app;
}

int main(int argc, const char **argv) {
    auto app = setupApplication();

    if(argc < 3) {
        std::cerr << "Usage: " << argv[0] << " module command [args...]" << std::endl;

        std::cerr << "Modules:" << std::endl;

        for(auto &m : app->modules) {
            std::cerr << "\t" << m->name << std::endl;

            for(auto &c : m->commands) {
                std::cerr << "\t\t" << c->definition() << std::endl;
            }
        }

        return 1;
    }

    const char *moduleName = argv[1];
    const char *commandName = argv[2];

    auto module = app->findModule(moduleName);
    if(module == nullptr) {
        std::cerr << "Module " << moduleName << " does not exist" << std::endl;
        return 1;
    }

    auto command = module->findCommand(commandName);
    if(command == nullptr) {
        std::cerr << "Command " << commandName << " does not exist" << std::endl;
        return 1;
    }

    xetool::CommandLineInput input;
    input.parse(command, argc - 3, argv + 3);

    std::vector<std::string> errors;

    if(!input.validate(errors)) {
        for(auto &err : errors) {
            std::cerr << err << std::endl;
        }

        return 1;
    }

    auto exit = command->execute(input);

    return exit;
}