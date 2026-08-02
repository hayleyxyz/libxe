cmake_minimum_required(VERSION 3.13)
project(bootloader_decrypt)

set(CMAKE_CXX_STANDARD 11)

add_executable(bootloader_decrypt
        bootloader_decrypt.cpp)

add_dependencies(bootloader_decrypt libxe)
target_link_libraries(bootloader_decrypt libxe)
target_include_directories(bootloader_decrypt PUBLIC ../)