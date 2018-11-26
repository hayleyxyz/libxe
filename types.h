//
// Created by yuikonnu on 29/10/2018.
//

#ifndef LIBXENON_TYPES_H
#define LIBXENON_TYPES_H

namespace {

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef	__signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;

typedef long long off_t;

#if defined(__SIZE_TYPE__)
typedef __SIZE_TYPE__ size_t;    /* sizeof() */
#else
typedef unsigned long size_t;	/* sizeof() */
#endif

typedef long ssize_t;    /* sizeof() */

}

#endif //LIBXENON_TYPES_H
