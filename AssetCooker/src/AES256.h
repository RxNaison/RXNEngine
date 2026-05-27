#pragma once

#include <cstdint>
#include <cstddef>

class AES256CTR 
{
private:
    uint32_t roundKeys[60];
    static const uint8_t sbox[256];
    static const uint8_t rcon[10];

    static inline uint8_t getSBoxValue(uint8_t num) { return sbox[num]; }
    void KeyExpansion(const uint8_t* key);
    static inline uint8_t xtime(uint8_t x) { return (x << 1) ^ (((x >> 7) & 1) * 0x1b); }
    void EncryptBlock(const uint8_t* in, uint8_t* out);
    void AddRoundKey(uint8_t state[4][4], int round);
    void SubBytes(uint8_t state[4][4]);
    void ShiftRows(uint8_t state[4][4]);
    void MixColumns(uint8_t state[4][4]);

public:
    AES256CTR(const uint8_t* key);
    void Crypt(uint64_t blockIndex, uint8_t* data, size_t length);
};
