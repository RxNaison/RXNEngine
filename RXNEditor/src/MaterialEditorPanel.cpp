#include "rxnpch.h"
#include "MaterialEditorPanel.h"
#include "RXNEngine/Core/Application.h"
#include "RXNEngine/Asset/AssetManager.h"
#include "RXNEngine/Serialization/MaterialSerializer.h"
#include "UI.h"
#include <imgui.h>

namespace RXNEditor {

    void MaterialEditorPanel::SetContext(const RXNEngine::Ref<RXNEngine::Material>& material, const std::string& filepath)
    {
        m_ActiveMaterial = material;
        m_ActiveFilepath = filepath;
        m_IsOpen = true;
    }

    void MaterialEditorPanel::OnImGuiRender()
    {
        if (!m_IsOpen || !m_ActiveMaterial) return;

        bool wasOpen = m_IsOpen;

        ImGui::Begin("Material Editor", &m_IsOpen);

        ImGui::TextDisabled("Editing: %s", m_ActiveFilepath.c_str());
        ImGui::Separator();

        auto& params = m_ActiveMaterial->GetParameters();

        if (UI::DrawColor4Control("Albedo Color", params.AlbedoColor))
            m_ActiveMaterial->SetTransparent(params.AlbedoColor.a < 1.0f);

        UI::DrawFloatControl("Roughness", params.Roughness, 0.0f, 1.0f);
        UI::DrawFloatControl("Metalness", params.Metalness, 0.0f, 1.0f);
        UI::DrawFloatControl("AO Strength", params.AO, 0.01f, 0.0f, 1.0f);
        UI::DrawColor3Control("Emissive Color", params.EmissiveColor);
        UI::DrawFloatControl("Tiling", params.Tiling, 0.1f);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Texture Maps");

        auto assetManager = RXNEngine::Application::Get().GetSubsystem<RXNEngine::AssetManager>();

        auto drawTextureSlot = [&](const char* label, RXNEngine::Ref<RXNEngine::Texture2D> currentTex, auto setterFunc)
            {
                ImGui::PushID(label);
                ImGui::Columns(2);
                ImGui::SetColumnWidth(0, 100.0f);
                ImGui::Text("%s", label);
                ImGui::NextColumn();

                if (currentTex)
                    ImGui::ImageButton(std::to_string((uint64_t)currentTex->GetRendererID()).c_str(), (ImTextureID)(uint64_t)currentTex->GetRendererID(), ImVec2(64, 64));
                else
                    ImGui::Button("Drop\nTexture", ImVec2(64, 64));

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        std::string path = (const char*)payload->Data;
                        if (path.ends_with(".png") || path.ends_with(".jpg"))
                        {
                            setterFunc(assetManager->GetTexture(path), path);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (currentTex)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Remove")) setterFunc(nullptr, "");
                }

                ImGui::Columns(1);
                ImGui::PopID();
                ImGui::Spacing();
            };

        drawTextureSlot("Albedo", m_ActiveMaterial->GetAlbedoMap(), [&](auto tex, auto p) { m_ActiveMaterial->SetAlbedoMap(tex, p); });
        drawTextureSlot("Normal", m_ActiveMaterial->GetNormalMap(), [&](auto tex, auto p) { m_ActiveMaterial->SetNormalMap(tex, p); });
        drawTextureSlot("Metalness", m_ActiveMaterial->GetMetalnessMap(), [&](auto tex, auto p) { m_ActiveMaterial->SetMetalnessMap(tex, p); });
        drawTextureSlot("Roughness", m_ActiveMaterial->GetRoughnessMap(), [&](auto tex, auto p) { m_ActiveMaterial->SetRoughnessMap(tex, p); });
        drawTextureSlot("Emissive", m_ActiveMaterial->GetEmissiveMap(), [&](auto tex, auto p) { m_ActiveMaterial->SetEmissiveMap(tex, p); });
        drawTextureSlot("AO", m_ActiveMaterial->GetAOMap(), [&](auto tex, auto p) { m_ActiveMaterial->SetAOMap(tex, p); });

        ImGui::Spacing();
        if (ImGui::Button("Save Asset", ImVec2(ImGui::GetContentRegionAvail().x, 40.0f)))
        {
            RXNEngine::MaterialSerializer serializer(m_ActiveMaterial);
            serializer.Serialize(m_ActiveFilepath);
        }

        ImGui::End();

        if (wasOpen && !m_IsOpen)
        {
            RXNEngine::MaterialSerializer serializer(m_ActiveMaterial);
            serializer.Deserialize(m_ActiveFilepath);

            m_ActiveMaterial = nullptr;
            m_ActiveFilepath.clear();
        }
    }
}