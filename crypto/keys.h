/*
* Created by yuikonnu on 02/11/2018.
*/

#ifndef LIBXE_KEYS_H
#define LIBXE_KEYS_H

#include "../types.h"

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
extern const uint8_t cpuRomKey[16];

/*
 * Found in xamd.dll in SDK
 * Used for HV expansions with the magic "SIGC"
 */
extern const uint8_t masterManufacturingKeyDevKit[0x390];

};
};
};

#endif //LIBXE_KEYS_H
