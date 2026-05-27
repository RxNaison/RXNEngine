#pragma once
#include "RXNEngine/Core/PakHeader.h"
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

struct AssetRecord
{
    std::string RelativePath;
    fs::path DiskPath;
    uint64_t UncompressedSize = 0;
};
