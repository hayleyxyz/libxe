//
// Created by github.com/hayleyxyz on 29/10/2018.
//

#pragma once

#include "types.h"

namespace xe {
namespace endian {

#ifndef LIBXENON_ENDIAN_BIG
#define LIBXENON_ENDIAN_LITTLE
#endif

inline uint16_t swapByteOrder_16(uint16_t value) {
#if defined(_MSC_VER) && !defined(_DEBUG)
    // The DLL version of the runtime lacks these functions (bug!?), but in a
   // release build they're replaced with BSWAP instructions anyway.
   return _byteswap_ushort(value);
#else
    uint16_t Hi = value << 8;
    uint16_t Lo = value >> 8;
    return Hi | Lo;
#endif
}

inline uint32_t swapByteOrder_32(uint32_t value) {
#if defined(__llvm__)
    return __builtin_bswap32(value);
#elif defined(_MSC_VER) && !defined(_DEBUG)
    return _byteswap_ulong(value);
 #else
   uint32_t Byte0 = value & 0x000000FF;
   uint32_t Byte1 = value & 0x0000FF00;
   uint32_t Byte2 = value & 0x00FF0000;
   uint32_t Byte3 = value & 0xFF000000;
   return (Byte0 << 24) | (Byte1 << 8) | (Byte2 >> 8) | (Byte3 >> 24);
#endif
}

/// swapByteOrder_64 - This function returns a byte-swapped representation of
/// the 64-bit argument.
inline uint64_t swapByteOrder_64(uint64_t value) {
#if defined(__llvm__)
    return __builtin_bswap64(value);
#elif defined(_MSC_VER) && !defined(_DEBUG)
    return _byteswap_uint64(value);
 #else
   uint64_t Hi = swapByteOrder_32(uint32_t(value));
   uint32_t Lo = swapByteOrder_32(uint32_t(value >> 32));
   return (Hi << 32) | Lo;
#endif
}

inline unsigned char  getSwappedBytes(unsigned char C) { return C; }
inline   signed char  getSwappedBytes(signed char C) { return C; }
inline          char  getSwappedBytes(char C) { return C; }

inline unsigned short getSwappedBytes(unsigned short C) { return swapByteOrder_16(C); }
inline   signed short getSwappedBytes(  signed short C) { return swapByteOrder_16(C); }

inline unsigned int   getSwappedBytes(unsigned int   C) { return swapByteOrder_32(C); }
inline   signed int   getSwappedBytes(  signed int   C) { return swapByteOrder_32(C); }

#if __LONG_MAX__ == __INT_MAX__
inline unsigned long  getSwappedBytes(unsigned long  C) { return swapByteOrder_32(C); }
 inline   signed long  getSwappedBytes(  signed long  C) { return swapByteOrder_32(C); }
#elif __LONG_MAX__ == __LONG_LONG_MAX__
inline unsigned long  getSwappedBytes(unsigned long  C) { return swapByteOrder_64(C); }
inline   signed long  getSwappedBytes(  signed long  C) { return swapByteOrder_64(C); }
#else
#error "Unknown long size!"
#endif

inline unsigned long long getSwappedBytes(unsigned long long C) { return swapByteOrder_64(C); }
inline signed long long getSwappedBytes(signed long long C) { return swapByteOrder_64(C); }

template<typename T>
inline T getBigEndianToNative(T C) {
#if defined(LIBXENON_ENDIAN_LITTLE)
    return getSwappedBytes(C);
#elif defined(LIBXENON_ENDIAN_BIG)
    return C;
#else
#error Neither LIBXENON_ENDIAN_LITTLE or LIBXENON_ENDIAN_BIG are defined
#endif
}

template<typename T>
inline void swapByteOrder(T &Value) {
    Value = getSwappedBytes(Value);
}

template<typename T>
inline void swapByteArray(T *Value, size_t count) {
    for(int i = 0; i < count; ++i) {
        Value[i] = swapByteOrder(Value[i]);
    }
}

template<typename T>
inline void swapByteArray(T &input, T &output, size_t count) {
    for(int i = 0; i < count; ++i) {
        output[i] = getSwappedBytes(input[i]);
    }
}

}
}
