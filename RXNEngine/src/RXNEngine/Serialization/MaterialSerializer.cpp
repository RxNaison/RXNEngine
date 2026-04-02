#include "rxnpch.h"
#include "MaterialSerializer.h"
#include "RXNEngine/Core/Application.h"
#include "RXNEngine/Asset/AssetManager.h"
#include "RXNEngine/Serialization/YamlHelpers.h"

#include <yaml-cpp/yaml.h>
#include <fstream>

namespace RXNEngine {

    MaterialSerializer::MaterialSerializer(const Ref<Material>& material)
        : m_Material(material)
    {}

    bool MaterialSerializer::Serialize(const std::string& filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Material" << YAML::Value << "Standard_PBR";

        auto& params = m_Material->GetParameters();

        out << YAML::Key << "Parameters";
        out << YAML::BeginMap;
        out << YAML::Key << "AlbedoColor" << YAML::Value << params.AlbedoColor;
        out << YAML::Key << "EmissiveColor" << YAML::Value << params.EmissiveColor;
        out << YAML::Key << "Roughness" << YAML::Value << params.Roughness;
        out << YAML::Key << "Metalness" << YAML::Value << params.Metalness;
        out << YAML::Key << "AO" << YAML::Value << params.AO;
        out << YAML::Key << "Tiling" << YAML::Value << params.Tiling;
        out << YAML::Key << "IsTransparent" << YAML::Value << m_Material->IsTransparent();
        out << YAML::EndMap;

        out << YAML::Key << "Textures";
        out << YAML::BeginMap;
        out << YAML::Key << "AlbedoMap" << YAML::Value << m_Material->GetAlbedoPath();
        out << YAML::Key << "NormalMap" << YAML::Value << m_Material->GetNormalPath();
        out << YAML::Key << "MetalMap" << YAML::Value << m_Material->GetMetalPath();
        out << YAML::Key << "RoughMap" << YAML::Value << m_Material->GetRoughPath();
        out << YAML::Key << "AOMap" << YAML::Value << m_Material->GetAOPath();
        out << YAML::Key << "EmissiveMap" << YAML::Value << m_Material->GetEmissivePath();
        out << YAML::EndMap;

        out << YAML::EndMap;

        std::ofstream fout(filepath);
        fout << out.c_str();
        return true;
    }

    bool MaterialSerializer::Deserialize(const std::string& filepath)
    {
        YAML::Node data;
        try { data = YAML::LoadFile(filepath); }
        catch (YAML::ParserException e) { RXN_CORE_ERROR("Failed to load .rxnmat file '{0}'\n     {1}", filepath, e.what()); return false; }

        if (!data["Material"]) return false;

        auto paramsNode = data["Parameters"];
        if (paramsNode)
        {
            auto& params = m_Material->GetParameters();
            params.AlbedoColor = paramsNode["AlbedoColor"].as<glm::vec4>();
            params.EmissiveColor = paramsNode["EmissiveColor"].as<glm::vec3>();
            params.Roughness = paramsNode["Roughness"].as<float>();
            params.Metalness = paramsNode["Metalness"].as<float>();
            params.AO = paramsNode["AO"].as<float>();
            params.Tiling = paramsNode["Tiling"].as<float>();
            m_Material->SetTransparent(paramsNode["IsTransparent"].as<bool>());
        }

        auto texturesNode = data["Textures"];
        if (texturesNode)
        {
            auto assetManager = Application::Get().GetSubsystem<AssetManager>();

            std::string albedoPath = texturesNode["AlbedoMap"].as<std::string>();
            if (!albedoPath.empty())
                m_Material->SetAlbedoMap(assetManager->GetTexture(albedoPath), albedoPath);

            std::string normalPath = texturesNode["NormalMap"].as<std::string>();
            if (!normalPath.empty())
                m_Material->SetNormalMap(assetManager->GetTexture(normalPath), normalPath);

            std::string metalPath = texturesNode["MetalMap"].as<std::string>();
            if (!metalPath.empty())
                m_Material->SetMetalnessMap(assetManager->GetTexture(metalPath), metalPath);

            std::string roughPath = texturesNode["RoughMap"].as<std::string>();
            if (!roughPath.empty())
                m_Material->SetRoughnessMap(assetManager->GetTexture(roughPath), roughPath);

            std::string aoPath = texturesNode["AOMap"].as<std::string>();
            if (!aoPath.empty())
                m_Material->SetAOMap(assetManager->GetTexture(aoPath), aoPath);

            std::string emissivePath = texturesNode["EmissiveMap"].as<std::string>();
            if (!emissivePath.empty())
                m_Material->SetEmissiveMap(assetManager->GetTexture(emissivePath), emissivePath);
        }

        return true;
    }
}