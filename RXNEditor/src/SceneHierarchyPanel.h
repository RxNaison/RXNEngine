#pragma once
#include <RXNEngine.h>
#include <imgui.h>

using namespace RXNEngine;

namespace RXNEditor {

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
    };
}