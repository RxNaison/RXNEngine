#pragma once

#include <cstdint>

class LZ4Encoder
{
public:
    static int LZ4_CompressBlock(const uint8_t* source, uint8_t* dest, int sourceLen);
};