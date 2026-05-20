#pragma once
#include <RXNEngine.h>
#include <imgui.h>
#include "CommandHistory.h"

using namespace RXNEngine;

namespace RXNEditor {

    template<typename T>
    class ComponentEditTracker
    {
    public:
        void BeginEdit(const T& currentComponentState)
        {
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !m_IsEditing)
            {
                m_OriginalState = currentComponentState;
                m_IsEditing = true;
            }
        }

        void EndEdit(RXNEngine::Ref<RXNEngine::Scene> scene, RXNEngine::Entity entity, const T& currentComponentState, bool isUIModifiedThisFrame)
        {
            if (isUIModifiedThisFrame)
                m_WasModified = true;

            if (ImGui::IsMouseReleased(0) && m_IsEditing)
            {
                if (m_WasModified)
                {
                    CommandHistory::Push(RXNEngine::CreateScope<ChangeComponentCommand<T>>(scene, entity.GetUUID(), m_OriginalState, currentComponentState));
                }

                m_IsEditing = false;
                m_WasModified = false;
            }
        }
    private:
        T m_OriginalState;
        bool m_IsEditing = false;
        bool m_WasModified = false;
    };

    class SceneHierarchyPanel
    {
    public:
        SceneHierarchyPanel() = default;
        SceneHierarchyPanel(const Ref<Scene>& context);

        void SetContext(const Ref<Scene>& context);
        void OnImGuiRender();

        Entity GetSelectedEntity() const { return m_SelectedEntities.empty() ? Entity{} : m_SelectedEntities.front(); }
        const std::vector<Entity>& GetSelectedEntities() const { return m_SelectedEntities; }

        void SetSelectedEntity(Entity entity);
        void ToggleSelection(Entity entity);
        bool IsEntitySelected(Entity entity) const;
        void ClearSelection();

        Entity ResolvePickedEntity(Entity entity);
        void SetMeshDropCallback(const std::function<void(const std::string&)>& callback) { m_MeshDropCallback = callback; }

    private:
        void DrawEntityNode(Entity entity);
        void DrawComponents(Entity entity);

        template<typename T, typename UIFunction>
        void DrawComponent(const std::string& name, Entity entity, UIFunction uiFunction);
    private:
        Ref<Scene> m_Context;
        std::vector<Entity> m_SelectedEntities;
        Entity m_SelectionContextAnchor;
        std::vector<Entity> m_VisibleNodes;
        std::unordered_set<uint64_t> m_ExpandedNodes;

        std::function<void(const std::string&)> m_MeshDropCallback;

        ComponentEditTracker<RXNEngine::TagComponent> m_TagTracker;
        ComponentEditTracker<RXNEngine::TransformComponent> m_TransformTracker;
        ComponentEditTracker<RXNEngine::CameraComponent> m_CameraTracker;
        ComponentEditTracker<RXNEngine::DirectionalLightComponent> m_DirLightTracker;
        ComponentEditTracker<RXNEngine::PointLightComponent> m_PointLightTracker;
        ComponentEditTracker<RXNEngine::SpotLightComponent> m_SpotLightTracker;
        ComponentEditTracker<RXNEngine::RigidbodyComponent> m_RigidbodyTracker;
        ComponentEditTracker<RXNEngine::BoxColliderComponent> m_BoxColliderTracker;
        ComponentEditTracker<RXNEngine::SphereColliderComponent> m_SphereColliderTracker;
        ComponentEditTracker<RXNEngine::CapsuleColliderComponent> m_CapsuleColliderTracker;
        ComponentEditTracker<RXNEngine::MeshColliderComponent> m_MeshColliderTracker;
        ComponentEditTracker<RXNEngine::CharacterControllerComponent> m_CCTTracker;
        ComponentEditTracker<RXNEngine::ScriptComponent> m_ScriptTracker;
        ComponentEditTracker<RXNEngine::AudioSourceComponent> m_AudioTracker;
        ComponentEditTracker<RXNEngine::UICanvasComponent> m_CanvasTracker;
        ComponentEditTracker<RXNEngine::UITransformComponent> m_UITransformTracker;
        ComponentEditTracker<RXNEngine::UIImageComponent> m_UIImageTracker;
        ComponentEditTracker<RXNEngine::UITextComponent> m_UITextTracker;
        ComponentEditTracker<RXNEngine::UIButtonComponent> m_UIButtonTracker;
    };
}