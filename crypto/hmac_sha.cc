/*
* Created by yuikonnu on 02/11/2018.
*/

#include "hmac_sha.h"
#include "keys.h"
#include "../endian.h"
#include "../io/file_stream.h"
#include <cryptopp/hmac.h>
#include <cryptopp/sha.h>
#include <cryptopp/rsa.h>
#include <cryptopp/rng.h>
#include <cryptopp/osrng.h>
#include <beecrypt/beecrypt.h>
#include <beecrypt/rsa.h>
#include <beecrypt/pkcs1.h>
#include <beecrypt/pkcs12.h>
#include <tomcrypt.h>


namespace xe {
namespace crypto {

void XeCryptHmacSha() {

    auto *xepkBE = const_cast<xe::crypto::keys::XenonRsaPrivate2048 *>(
            reinterpret_cast<const xe::crypto::keys::XenonRsaPrivate2048 *>(&xe::crypto::keys::masterManufacturingKeyDevKit[0])
    );



    CryptoPP::RSAES_PKCS1v15_Decryptor rsa();

    xe::crypto::keys::XenonRsaPrivate2048 xepkLE;
    xepkLE.rsa.u64len = xe::endian::getSwappedBytes(xepkBE->rsa.u64len);
    xepkLE.rsa.exponent = xe::endian::getSwappedBytes(xepkBE->rsa.exponent);
    xepkLE.rsa.reserved = xe::endian::getSwappedBytes(xepkBE->rsa.reserved);
    xe::endian::swapByteArray(xepkBE->modulus, xepkLE.modulus, 32);
    xe::endian::swapByteArray(xepkBE->p, xepkLE.p, 16);
    xe::endian::swapByteArray(xepkBE->q, xepkLE.q, 16);
    xe::endian::swapByteArray(xepkBE->dp, xepkLE.dp, 16);
    xe::endian::swapByteArray(xepkBE->dq, xepkLE.dq, 16);
    xe::endian::swapByteArray(xepkBE->cr, xepkLE.cr, 16);

    xe::io::FileStream fs;
    fs.open("/Users/oscar/Projects/X360/HvxDump 1.3/code.bin", xe::io::FileStream::in | xe::io::FileStream::binary);
    if(!fs.isOpen()) {
        return;
    }

    auto buflen = fs.length();
    auto buf = new uint8_t[buflen];
    fs.read(buf, buflen);
    fs.close();

    byte expHash[0x14] {
            0x11, 0x53, 0x03, 0x6b, 0x2a, 0x35, 0x5e, 0xb7, 0x26, 0x80, 0xda, 0xac, 0x98, 0xfa, 0xa1, 0xca, 0x45, 0x4b, 0xb6, 0x56
    };

    CryptoPP::AutoSeededRandomPool rng;

    CryptoPP::RSAFunction params;

    auto ir = params.IsRandomized();
    auto m = CryptoPP::Integer((const uint8_t *)&xepkBE->modulus[0], 32 * 8, CryptoPP::Integer::Signedness::UNSIGNED, CryptoPP::ByteOrder::BIG_ENDIAN_ORDER);
    auto e = CryptoPP::Integer(xe::endian::getSwappedBytes(xepkBE->rsa.exponent));
    params.Initialize(m, e);
    auto data = CryptoPP::Integer(&buf[0x30], 0x100);

    auto d = CryptoPP::Integer((const uint8_t *)&xepkBE->dp[0], 32 * 8, CryptoPP::Integer::Signedness::UNSIGNED, CryptoPP::ByteOrder::BIG_ENDIAN_ORDER);

    CryptoPP::InvertibleRSAFunction prsa;
    prsa.GenerateRandomWithKeySize(rng, 2048);

    CryptoPP::Integer in;

    CryptoPP::InvertibleRSAFunction prsai;
    auto genm = prsa.GetModulus();
    auto gene = prsa.GetPublicExponent();
    auto gend = prsa.GetPrivateExponent();
    prsai.Initialize(genm, gene, gend);

    auto applied = prsa.ApplyFunction(data);

    rsapk pubKey;
    rsapkInit(&pubKey);
    mpbset(&pubKey.n, 32, &xepkLE.modulus[0]);
    mpnsetw(&pubKey.e, xepkLE.rsa.exponent);

    mpnumber message;
    mpnsetbin(&message, &expHash[0], 0x14);

    uint64_t *sigPtr = reinterpret_cast<uint64_t *>(&buf[0x30]);

    mpnumber ct;
    //mpnsetbin(&ct, reinterpret_cast<const byte *>(&sigPtr[0]), 0x100);
    mpnset(&ct, 0x100 / 8, sigPtr);

    auto result = rsapub(
            &pubKey.n,
            &pubKey.e,
            &ct,
            &message
    );

    result = rsavrfy(
            &pubKey.n,
            &pubKey.e,
            &message,
            &ct
    );

    /*!\fn int rsavrfy(const mpbarrett* n, const mpnumber* e, const mpnumber* m, const mpnumber* c)
     * \brief This function performs a raw RSA verification.
     *
     * It verifies if ciphertext \a c was encrypted from cleartext \a m
     * with the private key matching the given public key \a (n, e).
     *
     * \param n The modulus.
     * \param e The public exponent.
     * \param m The cleartext message.
     * \param c The ciphertext message.
     * \retval 1 on success.
     * \retval 0 on failure.
     */
     //   BEECRYPTAPI
     //   int rsavrfy(const mpbarrett* n, const mpnumber* e,
     //               const mpnumber* m, const mpnumber* c);

    /*!\fn int rsapub(const mpbarrett* n, const mpnumber* e, const mpnumber* m, mpnumber* c)
     * \brief This function performs a raw RSA public key operation.
     *
     * This function can be used for encryption and verifying.
     *
     * It performs the following operation:
     * \li \f$c=m^{e}\ \textrm{mod}\ n\f$
     *
     * \param n The RSA modulus.
     * \param e The RSA public exponent.
     * \param m The message.
     * \param c The ciphertext.
     * \retval 0 on success.
     * \retval -1 on failure.
     */
    //BEECRYPTAPI
    //int rsapub(const mpbarrett* n, const mpnumber* e,
    //           const mpnumber* m, mpnumber* c);

//    auto pk = rsa.AccessPrivateKey();
    (void)0;
}

};
};