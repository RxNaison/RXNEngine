#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <algorithm>
#include <cstring>

#include "PakStructs.h"
#include "LZ4Encoder.h"
#include "AES256.h"

using namespace RXNEngine;

int main(int argc, char* argv[])
{
    std::cout << "RXNEngine Asset Cooker v1.1" << std::endl;

    std::string assetsDir = "res";
    std::string outputFile = "assets.rxnpak";
    std::string entryScene = "";
    std::string windowTitle = "RXN Engine Standalone Player";
    uint32_t windowWidth = 1280;
    uint32_t windowHeight = 720;
    uint32_t windowMode = 0; // 0 = Windowed

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--assets") == 0 && i + 1 < argc)
            assetsDir = argv[++i];
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            outputFile = argv[++i];
        else if (strcmp(argv[i], "--entry-scene") == 0 && i + 1 < argc)
            entryScene = argv[++i];
        else if (strcmp(argv[i], "--window-title") == 0 && i + 1 < argc)
            windowTitle = argv[++i];
        else if (strcmp(argv[i], "--window-width") == 0 && i + 1 < argc)
            windowWidth = std::stoul(argv[++i]);
        else if (strcmp(argv[i], "--window-height") == 0 && i + 1 < argc)
            windowHeight = std::stoul(argv[++i]);
        else if (strcmp(argv[i], "--window-mode") == 0 && i + 1 < argc)
            windowMode = std::stoul(argv[++i]);
    }

    if (!fs::exists(assetsDir))
    {
        std::cerr << "Error: Assets directory '" << assetsDir << "' does not exist!" << std::endl;
        return 1;
    }

    std::cout << "Target Directory:   " << assetsDir << std::endl;
    std::cout << "Output Archive:     " << outputFile << std::endl;

    if (!entryScene.empty())
        std::cout << "Baked Entry Scene:  " << entryScene << std::endl;

    std::cout << "Baked Window Config: [" << windowTitle << "] " << windowWidth << "x" << windowHeight << " (Mode " << windowMode << ")" << std::endl;

    std::vector<AssetRecord> records;
    for (const auto& entry : fs::recursive_directory_iterator(assetsDir))
    {
        if (entry.is_regular_file())
        {
            fs::path relPath = fs::relative(entry.path(), assetsDir);
            std::string relPathStr = relPath.generic_string();

            std::transform(relPathStr.begin(), relPathStr.end(), relPathStr.begin(), ::tolower);

            AssetRecord record;
            record.RelativePath = relPathStr;
            record.DiskPath = entry.path();
            record.UncompressedSize = fs::file_size(entry.path());
            records.push_back(record);
        }
    }

    std::cout << "Discovered " << records.size() << " assets to compile & package." << std::endl;

    std::ofstream out(outputFile, std::ios::binary);
    if (!out.is_open())
    {
        std::cerr << "Error: Failed to open output pak file: " << outputFile << std::endl;
        return 1;
    }

    PakHeader header;
    strncpy_s(header.EntryScenePath, entryScene.c_str(), 255);
    strncpy_s(header.WindowTitle, windowTitle.c_str(), 255);
    header.WindowWidth = windowWidth;
    header.WindowHeight = windowHeight;
    header.WindowMode = windowMode;
    header.EntryCount = (uint32_t)records.size();
    
    out.write((const char*)&header, sizeof(PakHeader));

    uint64_t headerSize = sizeof(PakHeader);
    uint64_t indexTableSize = records.size() * (256 + 8 + 8 + 8 + 1);
    uint64_t payloadStartOffset = headerSize + indexTableSize;

    struct CookedEntry
    {
        std::string Path;
        uint64_t Offset;
        uint64_t CompressedSize;
        uint64_t UncompressedSize;
        bool IsEncrypted;
        std::vector<uint8_t> Data;
    };

    std::vector<CookedEntry> cookedEntries;
    uint64_t currentPayloadOffset = payloadStartOffset;

    uint8_t aesKey[] = "RXNEngineSecureAssetPipelineKey32";
    AES256CTR aes(aesKey);

    for (const auto& rec : records)
    {
        std::cout << "Cooking: " << rec.RelativePath << " (" << rec.UncompressedSize << " bytes)..." << std::endl;

        std::ifstream srcFile(rec.DiskPath, std::ios::binary);
        std::vector<uint8_t> rawData(rec.UncompressedSize);
        srcFile.read((char*)rawData.data(), rec.UncompressedSize);
        srcFile.close();

        std::vector<uint8_t> compressedBuffer(rec.UncompressedSize * 2 + 16);
        int compressedSize = LZ4Encoder::LZ4_CompressBlock(rawData.data(), compressedBuffer.data(), (int)rec.UncompressedSize);

        CookedEntry cooked;
        cooked.Path = rec.RelativePath;
        cooked.UncompressedSize = rec.UncompressedSize;
        cooked.Offset = currentPayloadOffset;

        if (compressedSize >= (int)rec.UncompressedSize)
        {
            cooked.CompressedSize = rec.UncompressedSize;
            cooked.Data = rawData;
        }
        else
        {
            cooked.CompressedSize = compressedSize;
            cooked.Data.assign(compressedBuffer.begin(), compressedBuffer.begin() + compressedSize);
        }

        cooked.IsEncrypted = true;
        aes.Crypt(cooked.Offset, cooked.Data.data(), cooked.Data.size());

        cookedEntries.push_back(cooked);
        currentPayloadOffset += cooked.Data.size();
    }

    for (const auto& cooked : cookedEntries)
    {
        char entryPath[256] = { 0 };
        strncpy_s(entryPath, cooked.Path.c_str(), 255);
        out.write(entryPath, 256);
        out.write((const char*)&cooked.Offset, sizeof(uint64_t));
        out.write((const char*)&cooked.CompressedSize, sizeof(uint64_t));
        out.write((const char*)&cooked.UncompressedSize, sizeof(uint64_t));
        out.write((const char*)&cooked.IsEncrypted, sizeof(bool));
    }

    for (const auto& cooked : cookedEntries)
        out.write((const char*)cooked.Data.data(), cooked.Data.size());

    out.close();
    std::cout << "Successfully packed: " << outputFile << std::endl;
    std::cout << "Total Archive Size:  " << currentPayloadOffset << " bytes." << std::endl;

    return 0;
}
