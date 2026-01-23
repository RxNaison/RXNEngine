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

        Entity GetSelectedEntity() const { return m_SelectedEntity; }
        void SetSelectedEntity(Entity entity) { m_SelectedEntity = entity; }

    private:
        void DrawEntityNode(Entity entity);
        void DrawComponents(Entity entity);

        template<typename T, typename UIFunction>
        void DrawComponent(const std::string& name, Entity entity, UIFunction uiFunction);
    private:
        Ref<Scene> m_Context;
        Entity m_SelectedEntity;
    };
}