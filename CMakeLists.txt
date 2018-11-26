cmake_minimum_required(VERSION 3.9)
project(libxe)

set(CMAKE_CXX_STANDARD 11)

add_library(libxe SHARED
        compression/lzx.h
        compression/lzx.c
        compression/lzx_int.h
        io/file_stream.cc
        io/file_stream.h
        bootloaders/bootloader.cc
        bootloaders/bootloader.h
        endian.h
        sfcx/sfcx_image.cc
        sfcx/sfcx_image.h
        io/io.cc
        io/io.h
        types.h
        sfcx/page.cc
        sfcx/page.h
        sfcx/logical_stream.cpp
        sfcx/logical_stream.h
        io/stream.h
        rom/shadow_boot_rom.cc
        rom/shadow_boot_rom.h
        bootloaders/smc.cc
        bootloaders/smc.h
        io/memory_stream.cc
        io/memory_stream.h
        crypto/keys.h
        crypto/keys.cc crypto/hmac_sha.cc crypto/hmac_sha.h)

target_compile_definitions(libxe PUBLIC -DDEBUG=1)

target_link_libraries(libxe cryptopp)
add_subdirectory(xetool)

target_link_libraries(libxe beecrypt)
