#include "rxnpch.h"
#include "PhysicsMaterialSerializer.h"
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace RXNEngine {

    PhysicsMaterialSerializer::PhysicsMaterialSerializer(const Ref<PhysicsMaterial>& material)
        : m_Material(material) {}

    bool PhysicsMaterialSerializer::Serialize(const std::string& filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "PhysicsMaterial" << YAML::Value << "Standard";

        out << YAML::Key << "StaticFriction" << YAML::Value << m_Material->StaticFriction;
        out << YAML::Key << "DynamicFriction" << YAML::Value << m_Material->DynamicFriction;
        out << YAML::Key << "Restitution" << YAML::Value << m_Material->Restitution;

        out << YAML::EndMap;

        std::ofstream fout(filepath);
        fout << out.c_str();
        return true;
    }

    bool PhysicsMaterialSerializer::Deserialize(const std::string& filepath)
    {
        YAML::Node data;
        try
        {
            data = YAML::LoadFile(filepath);
        }
        catch (YAML::ParserException e)
        {
            RXN_CORE_ERROR("Failed to load .rxnphys file '{0}'\n     {1}", filepath, e.what()); return false;
        }

        if (!data["PhysicsMaterial"])
            return false;

        if (data["StaticFriction"])
            m_Material->StaticFriction = data["StaticFriction"].as<float>();
        if (data["DynamicFriction"])
            m_Material->DynamicFriction = data["DynamicFriction"].as<float>();
        if (data["Restitution"])
            m_Material->Restitution = data["Restitution"].as<float>();

        return true;
    }
}