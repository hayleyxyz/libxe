/*
* Created by github.com/hayleyxyz on 02/11/2018.
*/

#pragma once

#include "../types.h"

// WINDOWS_EXPORT_ALL_SYMBOLS (used to auto-export libxe's symbols on MSVC)
// only covers functions, not data, so these key tables need explicit
// dllexport/dllimport to be visible to consumers like xetool.
#if defined(_WIN32)
#ifdef LIBXE_BUILD
#define LIBXE_API __declspec(dllexport)
#else
#define LIBXE_API __declspec(dllimport)
#endif
#else
#define LIBXE_API
#endif

namespace xe {
namespace crypto {
namespace keys {

struct XenonRsa {
    uint32_t u64len;
    uint32_t exponent;
    uint64_t reserved;
};

struct XenonRsaPrivate2048 {
    XenonRsa rsa;
    uint64_t modulus[32];
    uint64_t p[16];           // [BnQwNe] Private prime P
    uint64_t q[16];           // [BnQwNe] Private prime Q
    uint64_t dp[16];          // [BnQwNe] Private exponent P
    uint64_t dq[16];          // [BnQwNe] Private exponent Q
    uint64_t cr[16];          // [BnQwNe] Private coefficient
};

// a.k.a 1BL key
extern LIBXE_API const uint8_t cpuRomKey[16];

/*
 * Found in xamd.dll in SDK
 * Used for HV expansions with the magic "SIGC"
 */
extern LIBXE_API const uint8_t masterManufacturingKeyDevKit[0x390];

};
};
};
