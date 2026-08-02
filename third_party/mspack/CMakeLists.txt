add_library(mspack STATIC
            lzxd.c
            lzx.h
            mspack.h
            system.h
            system.c)

target_include_directories(mspack PUBLIC .)
