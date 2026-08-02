cmake_minimum_required(VERSION 3.13)
project(lzxtool)

set(CMAKE_CXX_STANDARD 11)

add_executable(lzxtool
        lzxtool.cc)

add_dependencies(lzxtool libxe)
target_link_libraries(lzxtool libxe)
target_include_directories(lzxtool PUBLIC ../)