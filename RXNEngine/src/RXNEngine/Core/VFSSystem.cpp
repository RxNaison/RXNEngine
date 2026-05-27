#include "rxnpch.h"
#include "VFSSystem.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>

namespace RXNEngine {

    namespace {
        static int LZ4_DecompressBlock(const uint8_t* source, uint8_t* dest, int destLen)
        {
            int srcIp = 0;
            int destOp = 0;

            while (destOp < destLen)
            {
                uint8_t token = source[srcIp++];
                int literalLen = token >> 4;

                if (literalLen == 15)
                {
                    uint8_t s;
                    do
                    {
                        s = source[srcIp++];
                        literalLen += s;
                    } while (s == 255);
                }

                if (literalLen > 0)
                {
                    memcpy(dest + destOp, source + srcIp, literalLen);
                    destOp += literalLen;
                    srcIp += literalLen;
                }

                if (destOp >= destLen)
                    break;

                uint16_t offset = source[srcIp] | (source[srcIp + 1] << 8);
                srcIp += 2;

                int matchLen = token & 0x0F;
                if (matchLen == 15)
                {
                    uint8_t s;
                    do 
                    {
                        s = source[srcIp++];
                        matchLen += s;
                    } while (s == 255);
                }
                matchLen += 4; // Min match is 4

                int ref = destOp - offset;

                for (int i = 0; i < matchLen; ++i)
                    dest[destOp++] = dest[ref++];
            }

            return srcIp;
        }

        class AES256CTR
        {
        private:
            uint32_t roundKeys[60]; // 14 rounds, 4 * 15 = 60 words
            
            static const uint8_t sbox[256];
            static const uint8_t rcon[10];

            static inline uint8_t getSBoxValue(uint8_t num) { return sbox[num]; }

            void KeyExpansion(const uint8_t* key)
            {
                uint32_t temp;
                int i = 0;
                while (i < 8)
                {
                    roundKeys[i] = (key[4 * i] << 24) | (key[4 * i + 1] << 16) | (key[4 * i + 2] << 8) | key[4 * i + 3];
                    i++;
                }

                while (i < 60) 
                {
                    temp = roundKeys[i - 1];
                    if (i % 8 == 0)
                    {
                        temp = (temp << 8) | (temp >> 24);
                        temp = (getSBoxValue((temp >> 24) & 0xFF) << 24) ^
                               (getSBoxValue((temp >> 16) & 0xFF) << 16) ^
                               (getSBoxValue((temp >> 8) & 0xFF) << 8) ^
                               getSBoxValue(temp & 0xFF);
                        temp ^= (rcon[i / 8 - 1] << 24);
                    }
                    else if (i % 8 == 4)
                    {
                        temp = (getSBoxValue((temp >> 24) & 0xFF) << 24) ^
                               (getSBoxValue((temp >> 16) & 0xFF) << 16) ^
                               (getSBoxValue((temp >> 8) & 0xFF) << 8) ^
                               getSBoxValue(temp & 0xFF);
                    }
                    roundKeys[i] = roundKeys[i - 8] ^ temp;
                    i++;
                }
            }

            static inline uint8_t xtime(uint8_t x) 
            {
                return (x << 1) ^ (((x >> 7) & 1) * 0x1b);
            }

            void EncryptBlock(const uint8_t* in, uint8_t* out)
            {
                uint8_t state[4][4];
                for (int i = 0; i < 4; i++)
                {
                    for (int j = 0; j < 4; j++)
                        state[j][i] = in[i * 4 + j];
                }

                AddRoundKey(state, 0);

                for (int round = 1; round < 14; round++)
                {
                    SubBytes(state);
                    ShiftRows(state);
                    MixColumns(state);
                    AddRoundKey(state, round);
                }

                SubBytes(state);
                ShiftRows(state);
                AddRoundKey(state, 14);

                for (int i = 0; i < 4; i++)
                {
                    for (int j = 0; j < 4; j++) 
                        out[i * 4 + j] = state[j][i];
                }
            }

            void AddRoundKey(uint8_t state[4][4], int round)
            {
                for (int i = 0; i < 4; i++) 
                {
                    uint32_t keyWord = roundKeys[round * 4 + i];
                    state[0][i] ^= (keyWord >> 24) & 0xFF;
                    state[1][i] ^= (keyWord >> 16) & 0xFF;
                    state[2][i] ^= (keyWord >> 8) & 0xFF;
                    state[3][i] ^= keyWord & 0xFF;
                }
            }

            void SubBytes(uint8_t state[4][4])
            {
                for (int i = 0; i < 4; i++)
                {
                    for (int j = 0; j < 4; j++)
                        state[i][j] = getSBoxValue(state[i][j]);
                }
            }

            void ShiftRows(uint8_t state[4][4]) 
            {
                uint8_t temp;
                temp = state[1][0];
                state[1][0] = state[1][1];
                state[1][1] = state[1][2];
                state[1][2] = state[1][3];
                state[1][3] = temp;

                temp = state[2][0];
                state[2][0] = state[2][2];
                state[2][2] = temp;
                temp = state[2][1];
                state[2][1] = state[2][3];
                state[2][3] = temp;

                temp = state[3][3];
                state[3][3] = state[3][2];
                state[3][2] = state[3][1];
                state[3][1] = state[3][0];
                state[3][0] = temp;
            }

            void MixColumns(uint8_t state[4][4])
            {
                for (int i = 0; i < 4; i++) 
                {
                    uint8_t a = state[0][i];
                    uint8_t b = state[1][i];
                    uint8_t c = state[2][i];
                    uint8_t d = state[3][i];
                    state[0][i] = xtime(a) ^ xtime(b) ^ b ^ c ^ d;
                    state[1][i] = a ^ xtime(b) ^ xtime(c) ^ c ^ d;
                    state[2][i] = a ^ b ^ xtime(c) ^ xtime(d) ^ d;
                    state[3][i] = xtime(a) ^ a ^ b ^ c ^ xtime(d);
                }
            }

        public:
            AES256CTR(const uint8_t* key)
            {
                KeyExpansion(key);
            }

            void Crypt(uint64_t blockIndex, uint8_t* data, size_t length)
            {
                uint8_t iv[16] = { 0 };
                memcpy(iv, &blockIndex, sizeof(uint64_t));
                
                uint8_t keystream[16];
                size_t bytesProcessed = 0;

                while (bytesProcessed < length)
                {
                    EncryptBlock(iv, keystream);

                    for (int i = 15; i >= 8; i--)
                    {
                        if (++iv[i] != 0)
                            break;
                    }

                    size_t toXor = std::min(length - bytesProcessed, (size_t)16);

                    for (size_t i = 0; i < toXor; i++)
                        data[bytesProcessed + i] ^= keystream[i];

                    bytesProcessed += toXor;
                }
            }
        };

        const uint8_t AES256CTR::sbox[256] = {
            0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
            0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
            0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
            0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
            0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
            0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
            0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
            0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
            0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
            0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
            0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
            0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
            0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
            0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
            0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
            0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
        };

        const uint8_t AES256CTR::rcon[10] = {
            0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
        };
    }

    void VFSSystem::Init()
    {
        RXN_CORE_INFO("VFS Subsystem initialized.");

        if (std::filesystem::exists("core.rxnpak"))
            MountPak("core.rxnpak");

        if (std::filesystem::exists("assets.rxnpak"))
            MountPak("assets.rxnpak");

        if (std::filesystem::exists("audio.rxnpak"))
            MountPak("audio.rxnpak");

        if (std::filesystem::exists("patch_01.rxnpak"))
            MountPak("patch_01.rxnpak");
    }

    void VFSSystem::Shutdown()
    {
        for (auto& pak : m_MountedPaks)
        {
            if (pak.MappedViewAddress)
                UnmapViewOfFile(pak.MappedViewAddress);

            if (pak.FileMappingHandle)
                CloseHandle(pak.FileMappingHandle);

            if (pak.FileHandle) 
                CloseHandle(pak.FileHandle);
        }
        m_MountedPaks.clear();
        m_VirtualTree.clear();
        RXN_CORE_INFO("VFS Subsystem shutdown complete.");
    }

    bool VFSSystem::MountPak(const std::filesystem::path& pakPath)
    {
        std::wstring pakPathW = pakPath.wstring();
        HANDLE hFile = CreateFileW(pakPathW.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            RXN_CORE_WARN("VFS: Failed to open pak file: {0}", pakPath.string());
            return false;
        }

        LARGE_INTEGER size;
        if (!GetFileSizeEx(hFile, &size))
        {
            CloseHandle(hFile);
            return false;
        }

        HANDLE hMapping = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!hMapping)
        {
            CloseHandle(hFile);
            return false;
        }

        void* mappedAddress = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        if (!mappedAddress)
        {
            CloseHandle(hMapping);
            CloseHandle(hFile);
            return false;
        }

        const PakHeader* header = (const PakHeader*)mappedAddress;
        if (memcmp(header->Magic, "RXNPAK\0", 8) != 0)
        {
            RXN_CORE_WARN("VFS: Invalid pak file signature: {0}", pakPath.string());
            UnmapViewOfFile(mappedAddress);
            CloseHandle(hMapping);
            CloseHandle(hFile);
            return false;
        }

        uint32_t entryCount = header->EntryCount;
        const uint8_t* ptr = (const uint8_t*)mappedAddress + sizeof(PakHeader);

        MountedPak pak;
        pak.Path = pakPath;
        pak.FileHandle = hFile;
        pak.FileMappingHandle = hMapping;
        pak.MappedViewAddress = mappedAddress;
        pak.FileSize = size.QuadPart;
        pak.EntryScenePath = std::string(header->EntryScenePath);
        pak.WindowTitle = std::string(header->WindowTitle);
        pak.WindowWidth = header->WindowWidth;
        pak.WindowHeight = header->WindowHeight;
        pak.WindowMode = header->WindowMode;

        for (uint32_t i = 0; i < entryCount; i++)
        {
            char entryPath[256];
            memcpy(entryPath, ptr, 256);
            ptr += 256;

            uint64_t offset = *(uint64_t*)ptr;
            ptr += 8;
            uint64_t compressedSize = *(uint64_t*)ptr;
            ptr += 8;
            uint64_t uncompressedSize = *(uint64_t*)ptr;
            ptr += 8;
            bool isEncrypted = *(bool*)ptr;
            ptr += 1;

            PakEntry entry;
            entry.Offset = offset;
            entry.CompressedSize = compressedSize;
            entry.UncompressedSize = uncompressedSize;
            entry.IsEncrypted = isEncrypted;

            std::string relativePath(entryPath);
            std::transform(relativePath.begin(), relativePath.end(), relativePath.begin(), tolower);

            pak.Index[relativePath] = entry;
        }

        size_t newPakIndex = m_MountedPaks.size();
        m_MountedPaks.push_back(pak);

        for (const auto& [relativePath, entry] : pak.Index)
            m_VirtualTree[relativePath] = { newPakIndex, entry };

        if (!pak.EntryScenePath.empty())
            m_BakedEntryScene = pak.EntryScenePath;

        if (pak.WindowWidth > 0 && pak.WindowHeight > 0)
        {
            m_BakedWindowTitle = pak.WindowTitle;
            m_BakedWindowWidth = pak.WindowWidth;
            m_BakedWindowHeight = pak.WindowHeight;
            m_BakedWindowMode = pak.WindowMode;
        }

        RXN_CORE_INFO("VFS: Mounted pak '{0}' with {1} assets.", pakPath.string(), entryCount);
        return true;
    }

    std::vector<uint8_t> VFSSystem::ReadFile(const std::filesystem::path& path)
    {
        std::string normalizedPath = path.generic_string();
        std::transform(normalizedPath.begin(), normalizedPath.end(), normalizedPath.begin(), tolower);

        size_t assetsPos = normalizedPath.find("/assets/");

        if (assetsPos != std::string::npos)
            normalizedPath = normalizedPath.substr(assetsPos + 8);
        else if (normalizedPath.rfind("assets/", 0) == 0)
            normalizedPath = normalizedPath.substr(7);

        auto it = m_VirtualTree.find(normalizedPath);
        if (it == m_VirtualTree.end())
        {
            RXN_CORE_WARN("VFS: ReadFile not found in VirtualTree: '{0}' (normalized: '{1}')", path.string(), normalizedPath);
            return {};
        }

        size_t pakIndex = it->second.first;
        PakEntry entry = it->second.second;
        const MountedPak& pak = m_MountedPaks[pakIndex];

        const uint8_t* sourceData = (const uint8_t*)pak.MappedViewAddress + entry.Offset;
        std::vector<uint8_t> destBuffer(entry.UncompressedSize);

        if (entry.CompressedSize == entry.UncompressedSize)
        {
            if (entry.IsEncrypted)
            {
                memcpy(destBuffer.data(), sourceData, entry.UncompressedSize);
                
                uint8_t key[] = "RXNEngineSecureAssetPipelineKey32";
                AES256CTR aes(key);
                aes.Crypt(entry.Offset, destBuffer.data(), entry.UncompressedSize);
            }
            else
            {
                memcpy(destBuffer.data(), sourceData, entry.UncompressedSize);
            }
        }
        else
        {
            if (entry.IsEncrypted)
            {
                std::vector<uint8_t> compressedTemp(entry.CompressedSize);
                memcpy(compressedTemp.data(), sourceData, entry.CompressedSize);

                uint8_t key[] = "RXNEngineSecureAssetPipelineKey32";
                AES256CTR aes(key);
                aes.Crypt(entry.Offset, compressedTemp.data(), entry.CompressedSize);

                LZ4_DecompressBlock(compressedTemp.data(), destBuffer.data(), (int)entry.UncompressedSize);
            }
            else
            {
                LZ4_DecompressBlock(sourceData, destBuffer.data(), (int)entry.UncompressedSize);
            }
        }

        return destBuffer;
    }

    bool VFSSystem::FileExists(const std::filesystem::path& path)
    {
        std::string normalizedPath = path.generic_string();
        std::transform(normalizedPath.begin(), normalizedPath.end(), normalizedPath.begin(), tolower);

        size_t assetsPos = normalizedPath.find("/assets/");
        if (assetsPos != std::string::npos)
            normalizedPath = normalizedPath.substr(assetsPos + 8);
        else if (normalizedPath.rfind("assets/", 0) == 0)
            normalizedPath = normalizedPath.substr(7);

        return m_VirtualTree.find(normalizedPath) != m_VirtualTree.end();
    }
}
