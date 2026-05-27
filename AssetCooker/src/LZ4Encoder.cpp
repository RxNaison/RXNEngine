#include "LZ4Encoder.h"
#include <cstring>
#include <algorithm>

int LZ4Encoder::LZ4_CompressBlock(const uint8_t* source, uint8_t* dest, int sourceLen)
{
    int srcIp = 0;
    int destOp = 0;
    int anchor = 0;

    uint32_t hashShift = 20; // 4096 entries table
    int hashTab[4096] = { 0 };

    while (srcIp < sourceLen - 4)
    {
        uint32_t sequence = *(uint32_t*)(source + srcIp);
        uint32_t hash = (sequence * 2654435761U) >> hashShift;
        int ref = hashTab[hash];
        hashTab[hash] = srcIp;

        if (ref != 0 && (srcIp - ref) < 65536 && *(uint32_t*)(source + ref) == sequence)
        {
            int literalLen = srcIp - anchor;
            int tokenOp = destOp++;
            dest[tokenOp] = 0;

            if (literalLen >= 15)
            {
                dest[tokenOp] |= (15 << 4);
                int len = literalLen - 15;
                while (len >= 255)
                {
                    dest[destOp++] = 255;
                    len -= 255;
                }
                dest[destOp++] = len;
            }
            else
            {
                dest[tokenOp] |= (literalLen << 4);
            }

            if (literalLen > 0)
            {
                memcpy(dest + destOp, source + anchor, literalLen);
                destOp += literalLen;
            }

            uint16_t offset = (uint16_t)(srcIp - ref);
            dest[destOp++] = offset & 0xFF;
            dest[destOp++] = (offset >> 8) & 0xFF;

            srcIp += 4;
            ref += 4;
            int matchLen = 0;
            while (srcIp < sourceLen && source[srcIp] == source[ref])
            {
                srcIp++;
                ref++;
                matchLen++;
            }

            if (matchLen >= 15)
            {
                dest[tokenOp] |= 15;
                int len = matchLen - 15;
                while (len >= 255) 
                {
                    dest[destOp++] = 255;
                    len -= 255;
                }
                dest[destOp++] = len;
            }
            else
            {
                dest[tokenOp] |= matchLen;
            }

            anchor = srcIp;
        }
        else
        {
            srcIp++;
        }
    }

    int literalLen = sourceLen - anchor;
    if (literalLen > 0)
    {
        int tokenOp = destOp++;
        dest[tokenOp] = 0;
        if (literalLen >= 15)
        {
            dest[tokenOp] |= (15 << 4);
            int len = literalLen - 15;
            while (len >= 255)
            {
                dest[destOp++] = 255;
                len -= 255;
            }
            dest[destOp++] = len;
        }
        else
        {
            dest[tokenOp] |= (literalLen << 4);
        }
        memcpy(dest + destOp, source + anchor, literalLen);
        destOp += literalLen;
    }

    return destOp;
}
