#pragma once
#include "RXNEngine/Core/Base.h"
#include "RXNEngine/Core/Assert.h"
#include <string>
#include <filesystem>

namespace RXNEngine {

    struct ProjectConfig
    {
        std::string Name = "Untitled";
        std::filesystem::path StartScene;
        std::filesystem::path AssetDirectory;
        bool UseFMOD = true;
    };

    class Project
    {
    public:
        static const std::filesystem::path& GetProjectDirectory()
        {
            RXN_CORE_ASSERT(s_ActiveProject);
            return s_ActiveProject->m_ProjectDirectory;
        }

        static const std::filesystem::path& GetProjectFilePath()
        {
            RXN_CORE_ASSERT(s_ActiveProject);
            return s_ActiveProject->m_ProjectFilePath;
        }

        static std::filesystem::path GetAssetDirectory()
        {
            RXN_CORE_ASSERT(s_ActiveProject);
            return GetProjectDirectory() / s_ActiveProject->m_Config.AssetDirectory;
        }

        static std::filesystem::path GetAssetFileSystemPath(const std::filesystem::path& path)
        {
            RXN_CORE_ASSERT(s_ActiveProject);
            return GetAssetDirectory() / path;
        }

        static ProjectConfig& GetConfig() { return s_ActiveProject->m_Config; }
        static Ref<Project> GetActive() { return s_ActiveProject; }

        static void SetActive(Ref<Project> project) { s_ActiveProject = project; }
        static Ref<Project> New();

    private:
        ProjectConfig m_Config;
        std::filesystem::path m_ProjectDirectory;
        std::filesystem::path m_ProjectFilePath;

        inline static Ref<Project> s_ActiveProject;

        friend class ProjectSerializer;
    };
}