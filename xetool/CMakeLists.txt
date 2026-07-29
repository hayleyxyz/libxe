cmake_minimum_required(VERSION 3.13)
project(xetool)

set(CMAKE_CXX_STANDARD 11)

add_executable(xetool
        main.cc
        module.cc
        module.h
        application.cc
        application.h
        command.h
        command.cc
        argument_definition.h
        command_line_input.cpp
        command_line_input.h)

add_subdirectory(sfcx)
add_subdirectory(shadowboot)

add_dependencies(xetool libxe)
target_link_libraries(xetool libxe)
target_include_directories(xetool PUBLIC ../)