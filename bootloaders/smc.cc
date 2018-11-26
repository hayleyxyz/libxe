/*
* Created by yuikonnu on 02/11/2018.
*/

#include "smc.h"

void xe::bootloaders::SMC::read(xe::io::Stream &stream, size_t length) {
    if(this->data != nullptr) {
        delete[] this->data;
    }

    this->data = new uint8_t[length];
    stream.read(this->data, length);
    dataLength = length;
}

void xe::bootloaders::SMC::decrypt(uint8_t *output) {
    uint8_t key[4] = { 0x42, 0x75, 0x4E, 0x79 };
    int i = 0, length = dataLength, index = 0;

    while(length-- > 0) {
        int mod = data[index] * 0xFB;
        output[index] = data[index] ^ key[i];
        index++;
        i++; i &= 3;
        key[i] += (uint8_t)mod;
        key[(i + 1) & 3] += (uint8_t)(mod >> 8);
    }
}

void xe::bootloaders::SMC::writeDecrypted(xe::io::Stream &stream) {
    auto decrypted = new uint8_t[dataLength];
    decrypt(decrypted);
    stream.write(decrypted, dataLength);
    delete[] decrypted;
}


