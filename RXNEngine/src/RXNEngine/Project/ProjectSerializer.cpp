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
        catch (YAML::ParserException e)
        {
            RXN_CORE_ERROR("Failed to load project file '{0}'", filepath.string());
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

        m_Project->m_ProjectDirectory = filepath.parent_path();
        m_Project->m_ProjectFilePath = filepath;
        return true;
    }
}