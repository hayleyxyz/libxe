cmake_minimum_required(VERSION 3.13)
project(shadowboot_extract)

set(CMAKE_CXX_STANDARD 11)

add_executable(shadowboot_extract
        shadowboot_extract.cpp)

add_dependencies(shadowboot_extract libxe)
target_link_libraries(shadowboot_extract libxe)
target_include_directories(shadowboot_extract PUBLIC ../)