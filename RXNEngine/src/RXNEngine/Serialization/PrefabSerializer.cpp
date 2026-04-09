#include "rxnpch.h"
#include "PrefabSerializer.h"
#include "SceneSerializer.h"
#include "RXNEngine/Asset/AssetManager.h"
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace RXNEngine {

    static void SerializeEntityTree(YAML::Emitter& out, Entity entity, Scene* scene)
    {
        SceneSerializer::SerializeEntity(out, entity);

        auto& rc = entity.GetComponent<RelationshipComponent>();
        for (UUID childID : rc.Children)
        {
            Entity child = scene->GetEntityByUUID(childID);
            if (child)
                SerializeEntityTree(out, child, scene);
        }
    }

    void PrefabSerializer::Serialize(Entity entity, const std::string& filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Prefab" << YAML::Value << entity.GetComponent<TagComponent>().Tag;

        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

        SerializeEntityTree(out, entity, entity.GetScene());

        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream fout(filepath);
        fout << out.c_str();
    }

    Entity PrefabSerializer::Deserialize(Ref<Scene> targetScene, const std::string& filepath)
    {
        YAML::Node data;
        try 
        {
            data = YAML::LoadFile(filepath);
        }
        catch (YAML::ParserException e)
        {
            RXN_CORE_ERROR("Failed to load .rxnprefab file '{0}'", filepath); return {};
        }

        auto entities = data["Entities"];

        if (!entities)
            return {};

        std::unordered_map<uint64_t, UUID> oldToNewUUIDs;
        std::vector<Entity> spawnedEntities;

        for (auto entityNode : entities)
        {
            uint64_t oldUUID = entityNode["Entity"].as<uint64_t>();

            UUID newUUID = UUID();
            oldToNewUUIDs[oldUUID] = newUUID;

            std::string name = "Prefab Entity";
            if (auto tagComponent = entityNode["TagComponent"])
                name = tagComponent["Tag"].as<std::string>();

            Entity spawnedEntity = targetScene->CreateEntityWithUUID(newUUID, name);

            SceneSerializer::DeserializeComponentsToEntity(entityNode, spawnedEntity);

            spawnedEntities.push_back(spawnedEntity);
        }

        for (Entity entity : spawnedEntities)
        {
            auto& rc = entity.GetComponent<RelationshipComponent>();

            if (rc.ParentHandle != 0 && oldToNewUUIDs.find(rc.ParentHandle) != oldToNewUUIDs.end())
                rc.ParentHandle = oldToNewUUIDs[rc.ParentHandle];
            else
                rc.ParentHandle = 0;

            std::vector<UUID> newChildren;
            for (uint64_t oldChildID : rc.Children)
            {
                if (oldToNewUUIDs.find(oldChildID) != oldToNewUUIDs.end())
                    newChildren.push_back(oldToNewUUIDs[oldChildID]);
            }
            rc.Children = newChildren;
        }

        return spawnedEntities.empty() ? Entity{} : spawnedEntities[0];
    }
}