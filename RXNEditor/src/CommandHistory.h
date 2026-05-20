#pragma once
#include "RXNEngine/Physics/PhysicsWorld.h"

#include <RXNEngine.h>
#include <vector>
#include <string>

namespace RXNEditor {

    class EditorCommand
    {
    public:
        virtual ~EditorCommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;
    };

    class CommandHistory
    {
    public:
        static void Push(RXNEngine::Scope<EditorCommand> command);
        static void AddAndExecute(RXNEngine::Scope<EditorCommand> command);
        static void Undo();
        static void Redo();
        static void Clear();

    private:
        static std::vector<RXNEngine::Scope<EditorCommand>> s_Commands;
        static int s_CommandIndex;
    };

    template<typename T>
    class ChangeComponentCommand : public EditorCommand
    {
    public:
        ChangeComponentCommand(RXNEngine::Ref<RXNEngine::Scene> scene, RXNEngine::UUID entityID, const T& oldComponent, const T& newComponent)
            : m_Scene(scene), m_EntityID(entityID), m_OldComponent(oldComponent), m_NewComponent(newComponent) {}

        virtual void Execute() override
        {
            RXNEngine::Entity entity = m_Scene->GetEntityByUUID(m_EntityID);
            if (entity && entity.HasComponent<T>())
            {
                entity.GetComponent<T>() = m_NewComponent;
                if constexpr (std::is_same_v<T, RXNEngine::TransformComponent>)
                {
                    if (m_Scene->IsSimulating())
                        RXNEngine::Application::Get().GetSubsystem<RXNEngine::PhysicsWorld>()->SyncTransformToPhysics(entity);
                }
            }
        }

        virtual void Undo() override
        {
            RXNEngine::Entity entity = m_Scene->GetEntityByUUID(m_EntityID);
            if (entity && entity.HasComponent<T>())
            {
                entity.GetComponent<T>() = m_OldComponent;
                if constexpr (std::is_same_v<T, RXNEngine::TransformComponent>)
                {
                    if (m_Scene->IsSimulating())
                        RXNEngine::Application::Get().GetSubsystem<RXNEngine::PhysicsWorld>()->SyncTransformToPhysics(entity);
                }
            }
        }
    private:
        RXNEngine::Ref<RXNEngine::Scene> m_Scene;
        RXNEngine::UUID m_EntityID;
        T m_OldComponent;
        T m_NewComponent;
    };

    template<typename T>
    class AddComponentCommand : public EditorCommand
    {
    public:
        AddComponentCommand(RXNEngine::Ref<RXNEngine::Scene> scene, RXNEngine::UUID entityID, const T& componentData = T())
            : m_Scene(scene), m_EntityID(entityID), m_ComponentData(componentData) {}

        virtual void Execute() override
        {
            RXNEngine::Entity entity = m_Scene->GetEntityByUUID(m_EntityID);
            if (entity && !entity.HasComponent<T>())
                entity.AddComponent<T>(m_ComponentData);
        }

        virtual void Undo() override
        {
            RXNEngine::Entity entity = m_Scene->GetEntityByUUID(m_EntityID);
            if (entity && entity.HasComponent<T>())
                entity.RemoveComponent<T>();
        }
    private:
        RXNEngine::Ref<RXNEngine::Scene> m_Scene;
        RXNEngine::UUID m_EntityID;
        T m_ComponentData;
    };

    template<typename T>
    class RemoveComponentCommand : public EditorCommand
    {
    public:
        RemoveComponentCommand(RXNEngine::Ref<RXNEngine::Scene> scene, RXNEngine::UUID entityID, const T& componentData)
            : m_Scene(scene), m_EntityID(entityID), m_ComponentData(componentData) {}

        virtual void Execute() override
        {
            RXNEngine::Entity entity = m_Scene->GetEntityByUUID(m_EntityID);
            if (entity && entity.HasComponent<T>())
                entity.RemoveComponent<T>();
        }

        virtual void Undo() override
        {
            RXNEngine::Entity entity = m_Scene->GetEntityByUUID(m_EntityID);
            if (entity && !entity.HasComponent<T>())
                entity.AddComponent<T>(m_ComponentData);
        }
    private:
        RXNEngine::Ref<RXNEngine::Scene> m_Scene;
        RXNEngine::UUID m_EntityID;
        T m_ComponentData;
    };

    class CreateEntityCommand : public EditorCommand
    {
    public:
        CreateEntityCommand(RXNEngine::Ref<RXNEngine::Scene> scene, RXNEngine::UUID entityID, const std::string& name, RXNEngine::UUID parentID = 0);
        virtual void Execute() override;
        virtual void Undo() override;
    private:
        RXNEngine::Ref<RXNEngine::Scene> m_Scene;
        RXNEngine::UUID m_EntityID;
        std::string m_Name;
        RXNEngine::UUID m_ParentID;
    };

    class DeleteEntitiesCommand : public EditorCommand
    {
    public:
        DeleteEntitiesCommand(RXNEngine::Ref<RXNEngine::Scene> scene, const std::vector<RXNEngine::Entity>& entities);
        virtual void Execute() override;
        virtual void Undo() override;
    private:
        RXNEngine::Ref<RXNEngine::Scene> m_Scene;

        struct EntityDeleteData
        {
            RXNEngine::UUID EntityID;
            RXNEngine::UUID ParentID;
            std::string SerializedTreeData;
        };
        std::vector<EntityDeleteData> m_DeletedData;
    };

    class SpawnEntityTreeCommand : public EditorCommand
    {
    public:
        SpawnEntityTreeCommand(RXNEngine::Ref<RXNEngine::Scene> scene, RXNEngine::Entity rootEntity);
        virtual void Execute() override;
        virtual void Undo() override;
    private:
        RXNEngine::Ref<RXNEngine::Scene> m_Scene;
        RXNEngine::UUID m_EntityID;
        RXNEngine::UUID m_ParentID;
        std::string m_SerializedTreeData;
    };

    class ChangeParentCommand : public EditorCommand
    {
    public:
        ChangeParentCommand(RXNEngine::Ref<RXNEngine::Scene> scene, RXNEngine::UUID entityID, RXNEngine::UUID newParentID, RXNEngine::UUID oldParentID);
        virtual void Execute() override;
        virtual void Undo() override;
    private:
        RXNEngine::Ref<RXNEngine::Scene> m_Scene;
        RXNEngine::UUID m_EntityID;
        RXNEngine::UUID m_NewParentID;
        RXNEngine::UUID m_OldParentID;
    };

    class ChangeMultiTransformCommand : public EditorCommand
    {
    public:
        struct TransformData
        {
            RXNEngine::UUID EntityID;
            RXNEngine::TransformComponent OldTransform;
            RXNEngine::TransformComponent NewTransform;
        };

        ChangeMultiTransformCommand(RXNEngine::Ref<RXNEngine::Scene> scene, const std::vector<TransformData>& transforms);
        virtual void Execute() override;
        virtual void Undo() override;

    private:
        RXNEngine::Ref<RXNEngine::Scene> m_Scene;
        std::vector<TransformData> m_Transforms;
    };

    class ChangeMultiUITransformCommand : public EditorCommand
    {
    public:
        struct UITransformData
        {
            RXNEngine::UUID EntityID;
            RXNEngine::UITransformComponent OldTransform;
            RXNEngine::UITransformComponent NewTransform;
        };

        ChangeMultiUITransformCommand(RXNEngine::Ref<RXNEngine::Scene> scene, const std::vector<UITransformData>& transforms);
        virtual void Execute() override;
        virtual void Undo() override;

    private:
        RXNEngine::Ref<RXNEngine::Scene> m_Scene;
        std::vector<UITransformData> m_Transforms;
    };
}