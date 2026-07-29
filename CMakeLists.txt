cmake_minimum_required(VERSION 3.13)
project(libxe)

set(CMAKE_CXX_STANDARD 11)

# Put all build outputs (.exe/.dll/.lib/.pdb) for a given configuration in
# one folder, so binaries can be run/debugged directly without manually
# copying dependent DLLs around. On multi-config generators (Visual Studio)
# CMake appends the per-config subdirectory (Debug/Release/...) automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(libxe SHARED
        types.h
        endian.h)

set_target_properties(libxe PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_compile_definitions(libxe PUBLIC -DDEBUG=1)
target_compile_definitions(libxe PRIVATE LIBXE_BUILD)

add_subdirectory(compression)
add_subdirectory(io)
add_subdirectory(bootloaders)
add_subdirectory(sfcx)
add_subdirectory(rom)
add_subdirectory(crypto)
add_subdirectory(third_party/ExCrypt)

target_link_libraries(libxe ExCrypt)
add_subdirectory(xetool)
