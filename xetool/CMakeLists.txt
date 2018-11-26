cmake_minimum_required(VERSION 3.9)
project(xetool)

set(CMAKE_CXX_STANDARD 11)

add_executable(xetool
        main.cc module.cc module.h application.cc application.h command.h sfcx/extract_logical.cc sfcx/extract_logical.h argument_definition.h command_line_input.cpp command_line_input.h shadowboot/list.cc shadowboot/list.h command.cc shadowboot/extract.cc shadowboot/extract.h sfcx/extract.cc sfcx/extract.h)

add_dependencies(xetool libxe)
target_link_libraries(xetool libxe)
target_include_directories(xetool PUBLIC ../)