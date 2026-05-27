#pragma once

#include "RXNEngine/Core/Base.h"
#include "RXNEngine/Core/Subsystem.h"
#include "RXNEngine/Core/PakHeader.h"

#include <vector>
#include <string>
#include <unordered_map>
#include <filesystem>

namespace RXNEngine {

    struct PakEntry
    {
        uint64_t Offset = 0;
        uint64_t CompressedSize = 0;
        uint64_t UncompressedSize = 0;
        bool IsEncrypted = false;
    };

    struct MountedPak
    {
        std::filesystem::path Path;
        void* FileHandle = nullptr;
        void* FileMappingHandle = nullptr;
        void* MappedViewAddress = nullptr;
        uint64_t FileSize = 0;
        std::unordered_map<std::string, PakEntry> Index;
        
        std::string EntryScenePath;
        std::string WindowTitle;
        uint32_t WindowWidth = 0;
        uint32_t WindowHeight = 0;
        uint32_t WindowMode = 0;
    };

    class VFSSystem : public Subsystem 
    {
    public:
        VFSSystem() = default;
        virtual ~VFSSystem() = default;

        virtual void Init() override;
        virtual void Shutdown() override;

        bool MountPak(const std::filesystem::path& pakPath);
        
        std::vector<uint8_t> ReadFile(const std::filesystem::path& path);
        bool FileExists(const std::filesystem::path& path);

        const std::string& GetBakedEntryScene() const { return m_BakedEntryScene; }
        const std::string& GetBakedWindowTitle() const { return m_BakedWindowTitle; }
        uint32_t GetBakedWindowWidth() const { return m_BakedWindowWidth; }
        uint32_t GetBakedWindowHeight() const { return m_BakedWindowHeight; }
        uint32_t GetBakedWindowMode() const { return m_BakedWindowMode; }

    private:
        std::vector<MountedPak> m_MountedPaks;
        std::unordered_map<std::string, std::pair<size_t, PakEntry>> m_VirtualTree;

        std::string m_BakedEntryScene;
        std::string m_BakedWindowTitle;
        uint32_t m_BakedWindowWidth = 0;
        uint32_t m_BakedWindowHeight = 0;
        uint32_t m_BakedWindowMode = 0;
    };
}
