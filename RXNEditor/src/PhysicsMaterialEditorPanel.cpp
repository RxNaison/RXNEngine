#include "rxnpch.h"
#include "PhysicsMaterialEditorPanel.h"
#include "RXNEngine/Serialization/PhysicsMaterialSerializer.h"
#include "UI.h"
#include <imgui.h>

namespace RXNEditor {

    void PhysicsMaterialEditorPanel::SetContext(const RXNEngine::Ref<RXNEngine::PhysicsMaterial>& material, const std::string& filepath)
    {
        m_ActiveMaterial = material;
        m_ActiveFilepath = filepath;
        m_IsOpen = true;
    }

    void PhysicsMaterialEditorPanel::OnImGuiRender()
    {
        if (!m_IsOpen || !m_ActiveMaterial) return;

        bool wasOpen = m_IsOpen;

        ImGui::Begin("Physics Material Editor", &m_IsOpen);

        ImGui::TextDisabled("Editing: %s", m_ActiveFilepath.c_str());
        ImGui::Separator();

        UI::DrawFloatControl("Static Friction", m_ActiveMaterial->StaticFriction, 0.05f, 0.0f, 10.0f);
        UI::DrawFloatControl("Dynamic Friction", m_ActiveMaterial->DynamicFriction, 0.05f, 0.0f, 10.0f);
        UI::DrawFloatControl("Restitution (Bounciness)", m_ActiveMaterial->Restitution, 0.05f, 0.0f, 1.0f);

        ImGui::Spacing();
        if (ImGui::Button("Save Asset", ImVec2(ImGui::GetContentRegionAvail().x, 40.0f)))
        {
            RXNEngine::PhysicsMaterialSerializer serializer(m_ActiveMaterial);
            serializer.Serialize(m_ActiveFilepath);
        }

        ImGui::End();

        if (wasOpen && !m_IsOpen)
        {
            RXNEngine::PhysicsMaterialSerializer serializer(m_ActiveMaterial);
            serializer.Deserialize(m_ActiveFilepath);

            m_ActiveMaterial = nullptr;
            m_ActiveFilepath.clear();
        }
    }
}