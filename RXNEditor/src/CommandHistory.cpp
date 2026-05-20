#include "rxnpch.h"
#include "CommandHistory.h"
#include "RXNEngine/Serialization/SceneSerializer.h"
#include <yaml-cpp/yaml.h>

namespace RXNEditor {

    std::vector<RXNEngine::Scope<EditorCommand>> CommandHistory::s_Commands;
    int CommandHistory::s_CommandIndex = 0;

    void CommandHistory::Push(RXNEngine::Scope<EditorCommand> command)
    {
        if (s_CommandIndex < s_Commands.size())
            s_Commands.erase(s_Commands.begin() + s_CommandIndex, s_Commands.end());

        s_Commands.push_back(std::move(command));
        s_CommandIndex++;
    }

    void CommandHistory::AddAndExecute(RXNEngine::Scope<EditorCommand> command)
    {
        command->Execute();
        Push(std::move(command));
    }

    void CommandHistory::Undo()
    {
        if (s_CommandIndex > 0)
        {
            s_CommandIndex--;
            s_Commands[s_CommandIndex]->Undo();
        }
    }

    void CommandHistory::Redo()
    {
        if (s_CommandIndex < s_Commands.size())
        {
            s_Commands[s_CommandIndex]->Execute();
            s_CommandIndex++;
        }
    }

    void CommandHistory::Clear()
    {
        s_Commands.clear();
        s_CommandIndex = 0;
    }

    CreateEntityCommand::CreateEntityCommand(RXNEngine::Ref<RXNEngine::Scene> scene, RXNEngine::UUID entityID, const std::string& name, RXNEngine::UUID parentID)
        : m_Scene(scene), m_EntityID(entityID), m_Name(name), m_ParentID(parentID) {}

    void CreateEntityCommand::Execute()
    {
        if (!m_Scene->GetEntityByUUID(m_EntityID))
        {
            RXNEngine::Entity newEntity = m_Scene->CreateEntityWithUUID(m_EntityID, m_Name);

            if (m_ParentID != 0)
            {
                RXNEngine::Entity parent = m_Scene->GetEntityByUUID(m_ParentID);
                if (parent)
                    m_Scene->ParentEntity(newEntity, parent);
            }
        }
    }

    void CreateEntityCommand::Undo()
    {
        RXNEngine::Entity entity = m_Scene->GetEntityByUUID(m_EntityID);
        if (entity)
            m_Scene->DestroyEntity(entity);
    }

    static void SerializeEntityTree(YAML::Emitter& out, RXNEngine::Entity entity, RXNEngine::Scene* scene)
    {
        RXNEngine::SceneSerializer::SerializeEntity(out, entity);
        for (RXNEngine::UUID childID : entity.GetComponent<RXNEngine::RelationshipComponent>().Children)
        {
            RXNEngine::Entity child = scene->GetEntityByUUID(childID);
            if (child)
                SerializeEntityTree(out, child, scene);
        }
    }

    DeleteEntitiesCommand::DeleteEntitiesCommand(RXNEngine::Ref<RXNEngine::Scene> scene, const std::vector<RXNEngine::Entity>& entities)
        : m_Scene(scene)
    {
        std::vector<RXNEngine::Entity> topLevelEntities;

        for (auto entity : entities)
        {
            bool hasSelectedAncestor = false;
            RXNEngine::Entity current = entity;
            while (current.GetComponent<RXNEngine::RelationshipComponent>().ParentHandle != 0)
            {
                RXNEngine::Entity parent = m_Scene->GetEntityByUUID(current.GetComponent<RXNEngine::RelationshipComponent>().ParentHandle);
                if (!parent) 
                    break;

                if (std::find(entities.begin(), entities.end(), parent) != entities.end())
                {
                    hasSelectedAncestor = true;
                    break;
                }
                current = parent;
            }
            if (!hasSelectedAncestor)
                topLevelEntities.push_back(entity);
        }

        for (auto entity : topLevelEntities)
        {
            EntityDeleteData data;
            data.EntityID = entity.GetUUID();
            data.ParentID = entity.GetComponent<RXNEngine::RelationshipComponent>().ParentHandle;

            YAML::Emitter out;
            out << YAML::BeginSeq;
            SerializeEntityTree(out, entity, scene.get());
            out << YAML::EndSeq;

            data.SerializedTreeData = out.c_str();
            m_DeletedData.push_back(data);
        }
    }

    void DeleteEntitiesCommand::Execute()
    {
        for (auto& data : m_DeletedData)
        {
            RXNEngine::Entity entity = m_Scene->GetEntityByUUID(data.EntityID);
            if (entity)
                m_Scene->DestroyEntity(entity);
        }
    }

    void DeleteEntitiesCommand::Undo()
    {
        for (auto& data : m_DeletedData)
        {
            YAML::Node yamlData = YAML::Load(data.SerializedTreeData);
            std::vector<RXNEngine::Entity> restoredEntities;

            for (auto entityNode : yamlData)
            {
                uint64_t uuid = entityNode["Entity"].as<uint64_t>();
                std::string name = entityNode["TagComponent"] ? entityNode["TagComponent"]["Tag"].as<std::string>() : "Entity";

                RXNEngine::Entity restored = m_Scene->CreateEntityWithUUID(uuid, name);
                RXNEngine::SceneSerializer::DeserializeComponentsToEntity(entityNode, restored);
                restoredEntities.push_back(restored);
            }

            if (!restoredEntities.empty() && data.ParentID != 0)
            {
                RXNEngine::Entity parent = m_Scene->GetEntityByUUID(data.ParentID);
                if (parent)
                    m_Scene->ParentEntity(restoredEntities[0], parent);
            }
        }
    }

    SpawnEntityTreeCommand::SpawnEntityTreeCommand(RXNEngine::Ref<RXNEngine::Scene> scene, RXNEngine::Entity rootEntity)
        : m_Scene(scene), m_EntityID(rootEntity.GetUUID())
    {
        m_ParentID = rootEntity.GetComponent<RXNEngine::RelationshipComponent>().ParentHandle;

        YAML::Emitter out;
        out << YAML::BeginSeq;
        SerializeEntityTree(out, rootEntity, scene.get());
        out << YAML::EndSeq;
        m_SerializedTreeData = out.c_str();
    }

    void SpawnEntityTreeCommand::Execute()
    {
        if (m_Scene->GetEntityByUUID(m_EntityID))
            return;

        YAML::Node data = YAML::Load(m_SerializedTreeData);
        std::vector<RXNEngine::Entity> restoredEntities;

        for (auto entityNode : data)
        {
            uint64_t uuid = entityNode["Entity"].as<uint64_t>();
            std::string name = entityNode["TagComponent"] ? entityNode["TagComponent"]["Tag"].as<std::string>() : "Entity";

            RXNEngine::Entity restored = m_Scene->CreateEntityWithUUID(uuid, name);
            RXNEngine::SceneSerializer::DeserializeComponentsToEntity(entityNode, restored);
            restoredEntities.push_back(restored);
        }

        if (!restoredEntities.empty() && m_ParentID != 0)
        {
            RXNEngine::Entity parent = m_Scene->GetEntityByUUID(m_ParentID);
            if (parent)
                m_Scene->ParentEntity(restoredEntities[0], parent);
        }
    }

    void SpawnEntityTreeCommand::Undo()
    {
        RXNEngine::Entity entity = m_Scene->GetEntityByUUID(m_EntityID);
        if (entity)
            m_Scene->DestroyEntity(entity);
    }

    ChangeParentCommand::ChangeParentCommand(RXNEngine::Ref<RXNEngine::Scene> scene, RXNEngine::UUID entityID, RXNEngine::UUID newParentID, RXNEngine::UUID oldParentID)
        : m_Scene(scene), m_EntityID(entityID), m_NewParentID(newParentID), m_OldParentID(oldParentID) {}

    void ChangeParentCommand::Execute()
    {
        RXNEngine::Entity ent = m_Scene->GetEntityByUUID(m_EntityID);
        RXNEngine::Entity newP = m_Scene->GetEntityByUUID(m_NewParentID);
        if (ent)
            m_Scene->ParentEntity(ent, newP);
    }

    void ChangeParentCommand::Undo()
    {
        RXNEngine::Entity ent = m_Scene->GetEntityByUUID(m_EntityID);
        RXNEngine::Entity oldP = m_Scene->GetEntityByUUID(m_OldParentID);
        if (ent)
            m_Scene->ParentEntity(ent, oldP);
    }

    ChangeMultiTransformCommand::ChangeMultiTransformCommand(RXNEngine::Ref<RXNEngine::Scene> scene, const std::vector<TransformData>& transforms)
        : m_Scene(scene), m_Transforms(transforms) {}

    void ChangeMultiTransformCommand::Execute()
    {
        for (const auto& data : m_Transforms)
        {
            RXNEngine::Entity entity = m_Scene->GetEntityByUUID(data.EntityID);
            if (entity && entity.HasComponent<RXNEngine::TransformComponent>())
            {
                entity.GetComponent<RXNEngine::TransformComponent>() = data.NewTransform;
                if (m_Scene->IsSimulating())
                    RXNEngine::Application::Get().GetSubsystem<RXNEngine::PhysicsWorld>()->SyncTransformToPhysics(entity);
            }
        }
    }

    void ChangeMultiTransformCommand::Undo()
    {
        for (const auto& data : m_Transforms)
        {
            RXNEngine::Entity entity = m_Scene->GetEntityByUUID(data.EntityID);
            if (entity && entity.HasComponent<RXNEngine::TransformComponent>())
            {
                entity.GetComponent<RXNEngine::TransformComponent>() = data.OldTransform;
                if (m_Scene->IsSimulating())
                    RXNEngine::Application::Get().GetSubsystem<RXNEngine::PhysicsWorld>()->SyncTransformToPhysics(entity);
            }
        }
    }

    ChangeMultiUITransformCommand::ChangeMultiUITransformCommand(RXNEngine::Ref<RXNEngine::Scene> scene, const std::vector<UITransformData>& transforms)
        : m_Scene(scene), m_Transforms(transforms) {}

    void ChangeMultiUITransformCommand::Execute()
    {
        for (const auto& data : m_Transforms)
        {
            RXNEngine::Entity entity = m_Scene->GetEntityByUUID(data.EntityID);
            if (entity && entity.HasComponent<RXNEngine::UITransformComponent>())
            {
                entity.GetComponent<RXNEngine::UITransformComponent>() = data.NewTransform;
                entity.GetComponent<RXNEngine::UITransformComponent>().IsDirty = true;
            }
        }
    }

    void ChangeMultiUITransformCommand::Undo()
    {
        for (const auto& data : m_Transforms)
        {
            RXNEngine::Entity entity = m_Scene->GetEntityByUUID(data.EntityID);
            if (entity && entity.HasComponent<RXNEngine::UITransformComponent>())
            {
                entity.GetComponent<RXNEngine::UITransformComponent>() = data.OldTransform;
                entity.GetComponent<RXNEngine::UITransformComponent>().IsDirty = true;
            }
        }
    }
}