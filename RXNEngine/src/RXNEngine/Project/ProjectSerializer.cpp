#include "rxnpch.h"
#include "ProjectSerializer.h"
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace RXNEngine {

    Ref<Project> Project::New()
    {
        s_ActiveProject = CreateRef<Project>();
        return s_ActiveProject;
    }

    ProjectSerializer::ProjectSerializer(Ref<Project> project)
        : m_Project(project) {}

    bool ProjectSerializer::Serialize(const std::filesystem::path& filepath)
    {
        m_Project->m_ProjectDirectory = filepath.parent_path();
        m_Project->m_ProjectFilePath = filepath;

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Project" << YAML::Value;
        out << YAML::BeginMap;
        out << YAML::Key << "Name" << YAML::Value << m_Project->m_Config.Name;
        out << YAML::Key << "StartScene" << YAML::Value << m_Project->m_Config.StartScene.string();
        out << YAML::Key << "AssetDirectory" << YAML::Value << m_Project->m_Config.AssetDirectory.string();
        out << YAML::Key << "UseFMOD" << YAML::Value << m_Project->m_Config.UseFMOD;

        out << YAML::Key << "WindowTitle" << YAML::Value << m_Project->m_Config.WindowTitle;
        out << YAML::Key << "WindowWidth" << YAML::Value << m_Project->m_Config.WindowWidth;
        out << YAML::Key << "WindowHeight" << YAML::Value << m_Project->m_Config.WindowHeight;
        out << YAML::Key << "WindowMode" << YAML::Value << m_Project->m_Config.WindowMode;
        out << YAML::EndMap;
        out << YAML::EndMap;

        std::ofstream fout(filepath);
        fout << out.c_str();

        return true;
    }

    bool ProjectSerializer::Deserialize(const std::filesystem::path& filepath)
    {
        YAML::Node data;
        try
        {
            data = YAML::LoadFile(filepath.string());
        }
        catch (const std::exception& e)
        {
            RXN_CORE_ERROR("Failed to load project file '{0}'\n     {1}", filepath.string(), e.what());
            return false;
        }

        auto projectNode = data["Project"];
        if (!projectNode)
            return false;

        m_Project->m_Config.Name = projectNode["Name"].as<std::string>();
        m_Project->m_Config.StartScene = projectNode["StartScene"].as<std::string>();
        m_Project->m_Config.AssetDirectory = projectNode["AssetDirectory"].as<std::string>();

        if (projectNode["UseFMOD"])
            m_Project->m_Config.UseFMOD = projectNode["UseFMOD"].as<bool>();

        if (projectNode["WindowTitle"])
            m_Project->m_Config.WindowTitle = projectNode["WindowTitle"].as<std::string>();
        if (projectNode["WindowWidth"])
            m_Project->m_Config.WindowWidth = projectNode["WindowWidth"].as<uint32_t>();
        if (projectNode["WindowHeight"])
            m_Project->m_Config.WindowHeight = projectNode["WindowHeight"].as<uint32_t>();
        if (projectNode["WindowMode"])
            m_Project->m_Config.WindowMode = projectNode["WindowMode"].as<uint32_t>();

        m_Project->m_ProjectDirectory = filepath.parent_path();
        m_Project->m_ProjectFilePath = filepath;
        return true;
    }
}