#include "rxnpch.h"
#include "SceneHierarchyPanel.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

namespace RXNEditor {

    static void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f)
    {
        ImGui::PushID(label.c_str());

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text(label.c_str());
        ImGui::NextColumn();

        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

        float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

        if (ImGui::Button("X", buttonSize))
            values.x = resetValue;

        ImGui::SameLine();
        ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();
        ImGui::SameLine();

        if (ImGui::Button("Y", buttonSize))
            values.y = resetValue;

        ImGui::SameLine();
        ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();
        ImGui::SameLine();

        if (ImGui::Button("Z", buttonSize))
            values.z = resetValue;

        ImGui::SameLine();
        ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();
        ImGui::Columns(1);
        ImGui::PopID();
    }

    SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context)
    {
        SetContext(context);
    }

    void SceneHierarchyPanel::SetContext(const Ref<Scene>& context)
    {
        m_Context = context;
        m_SelectedEntity = {};
    }

    void SceneHierarchyPanel::OnImGuiRender()
    {
        // --- Hierarchy Panel ---
        ImGui::Begin("Scene Hierarchy");

        if (m_Context)
        {
            m_Context->GetRaw().view<entt::entity>().each([&](auto entityID)
                {
                    Entity entity{ entityID, m_Context.get() };
                    DrawEntityNode(entity);
                });

            if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight))
            {
                if (ImGui::MenuItem("Create New Entity"))
                    m_Context->CreateEntity("New Entity");

                ImGui::EndPopup();
            }
        }
        ImGui::End();

        // --- Properties Panel ---
        ImGui::Begin("Properties");
        if (m_SelectedEntity)
            DrawComponents(m_SelectedEntity);
        ImGui::End();
    }

    void SceneHierarchyPanel::DrawEntityNode(Entity entity)
    {
        auto& tag = entity.GetComponent<TagComponent>().Tag;

        ImGuiTreeNodeFlags flags = ((m_SelectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
        flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

        bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entity.GetUUID(), flags, tag.c_str());

        if (ImGui::IsItemClicked())
        {
            m_SelectedEntity = entity;
        }

        bool entityDeleted = false;
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Delete Entity"))
                entityDeleted = true;

            ImGui::EndPopup();
        }

        if (opened)
        {
            ImGui::TreePop();
        }

        if (entityDeleted)
        {
            m_Context->DestroyEntity(entity);
            if (m_SelectedEntity == entity)
                m_SelectedEntity = {};
        }
    }

    void SceneHierarchyPanel::DrawComponents(Entity entity)
    {
        // --- 1. Tag Component (Entity Name) ---
        if (entity.HasComponent<TagComponent>())
        {
            auto& tag = entity.GetComponent<TagComponent>().Tag;

            char buffer[256];
            memset(buffer, 0, sizeof(buffer));
            strcpy_s(buffer, sizeof(buffer), tag.c_str());
            if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
            {
                tag = std::string(buffer);
            }
        }

        // Add Component Button (Right aligned)
        ImGui::SameLine();
        ImGui::PushItemWidth(-1);
        if (ImGui::Button("Add Component"))
            ImGui::OpenPopup("AddComponent");

        if (ImGui::BeginPopup("AddComponent"))
        {
            if (!m_SelectedEntity.HasComponent<CameraComponent>())
            {
                if (ImGui::MenuItem("Camera"))
                {
                    m_SelectedEntity.AddComponent<CameraComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }
            if (!m_SelectedEntity.HasComponent<MeshComponent>())
            {
                if (ImGui::MenuItem("Mesh"))
                {
                    m_SelectedEntity.AddComponent<MeshComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }
            if (!m_SelectedEntity.HasComponent<DirectionalLightComponent>())
            {
                if (ImGui::MenuItem("Directional Light"))
                {
                    m_SelectedEntity.AddComponent<DirectionalLightComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }
            if (!m_SelectedEntity.HasComponent<PointLightComponent>())
            {
                if (ImGui::MenuItem("Point Light"))
                {
                    m_SelectedEntity.AddComponent<PointLightComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }
            if (!m_SelectedEntity.HasComponent<SkyboxComponent>())
            {
                if (ImGui::MenuItem("Skybox"))
                {
                    m_SelectedEntity.AddComponent<SkyboxComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
        ImGui::PopItemWidth();


        // --- 2. Transform Component ---
        DrawComponent<TransformComponent>("Transform", entity, [](auto& component)
            {
                DrawVec3Control("Translation", component.Translation);

                glm::vec3 rotation = glm::degrees(component.Rotation);
                DrawVec3Control("Rotation", rotation);
                component.Rotation = glm::radians(rotation);

                DrawVec3Control("Scale", component.Scale, 1.0f);
            });


        // --- 3. Camera Component ---
        DrawComponent<CameraComponent>("Camera", entity, [](auto& component)
            {
                auto& camera = component.Camera;

                ImGui::Checkbox("Primary", &component.Primary);

                const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
                const char* currentProjectionTypeString = projectionTypeStrings[(int)camera.GetProjectionType()];

                if (ImGui::BeginCombo("Projection", currentProjectionTypeString))
                {
                    for (int i = 0; i < 2; i++)
                    {
                        bool isSelected = currentProjectionTypeString == projectionTypeStrings[i];
                        if (ImGui::Selectable(projectionTypeStrings[i], isSelected))
                        {
                            currentProjectionTypeString = projectionTypeStrings[i];
                            camera.SetProjectionType((SceneCamera::ProjectionMode)i);
                        }

                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (camera.GetProjectionType() == SceneCamera::ProjectionMode::Perspective)
                {
                    float Fov = glm::degrees(camera.GetPerspectiveFOV());
                    if (ImGui::DragFloat("FOV", &Fov))
                        camera.SetPerspectiveFOV(glm::radians(Fov));

                    float orthoNear = camera.GetPerspectiveNearClip();
                    if (ImGui::DragFloat("Near", &orthoNear))
                        camera.SetPerspectiveNearClip(orthoNear);

                    float orthoFar = camera.GetPerspectiveFarClip();
                    if (ImGui::DragFloat("Far", &orthoFar))
                        camera.SetPerspectiveFarClip(orthoFar);
                }

                if (camera.GetProjectionType() == SceneCamera::ProjectionMode::Orthographic)
                {
                    float orthoSize = camera.GetOrthographicSize();
                    if (ImGui::DragFloat("Size", &orthoSize))
                        camera.SetOrthographicSize(orthoSize);

                    float orthoNear = camera.GetOrthographicNearClip();
                    if (ImGui::DragFloat("Near", &orthoNear))
                        camera.SetOrthographicNearClip(orthoNear);

                    float orthoFar = camera.GetOrthographicFarClip();
                    if (ImGui::DragFloat("Far", &orthoFar))
                        camera.SetOrthographicFarClip(orthoFar);
                }
            });


        // --- 4. Mesh Component (Material Editing) ---
        DrawComponent<MeshComponent>("Mesh", entity, [](auto& component)
            {
                ImGui::Columns(2);
                ImGui::SetColumnWidth(0, 100.0f);
                ImGui::Text("Mesh");
                ImGui::NextColumn();
                ImGui::Button("Drop Model Here", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));
                // TODO: Drag and Drop payload target here later
                ImGui::Columns(1);

                if (component.MaterialInstance)
                {
                    ImGui::Separator();
                    ImGui::Text("Material Properties");

                    auto params = component.MaterialInstance->GetParameters();

                    if (ImGui::ColorEdit4("Albedo", glm::value_ptr(params.AlbedoColor)))
                        component.MaterialInstance->SetAlbedoColor(params.AlbedoColor);

                    if (ImGui::SliderFloat("Roughness", &params.Roughness, 0.0f, 1.0f))
                        component.MaterialInstance->SetRoughness(params.Roughness);

                    if (ImGui::SliderFloat("Metalness", &params.Metalness, 0.0f, 1.0f))
                        component.MaterialInstance->SetMetalness(params.Metalness);

                    if (ImGui::DragFloat("AO Strength", &params.AO, 0.01f, 0.0f, 1.0f))
                        component.MaterialInstance->SetAO(params.AO);

                    if (ImGui::ColorEdit3("Emissive", glm::value_ptr(params.EmissiveColor)))
                        component.MaterialInstance->SetEmissiveColor(params.EmissiveColor);
                }
            });


        // --- 5. Directional Light ---
        DrawComponent<DirectionalLightComponent>("Directional Light", entity, [](auto& component)
            {
                ImGui::ColorEdit3("Color", glm::value_ptr(component.Color));
                ImGui::DragFloat("Intensity", &component.Intensity, 0.1f, 0.0f, 100.0f);
            });


        // --- 6. Point Light ---
        DrawComponent<PointLightComponent>("Point Light", entity, [](auto& component)
            {
                ImGui::ColorEdit3("Color", glm::value_ptr(component.Color));
                ImGui::DragFloat("Intensity", &component.Intensity, 0.1f, 0.0f, 100.0f);
                ImGui::DragFloat("Radius", &component.Radius, 0.1f, 0.0f, 1000.0f);
                ImGui::DragFloat("Falloff", &component.Falloff, 0.01f, 0.0f, 1.0f);
            });


        // --- 7. Skybox Component ---
        DrawComponent<SkyboxComponent>("Skybox", entity, [](auto& component)
            {
                ImGui::DragFloat("Intensity", &component.Intensity, 0.1f, 0.0f, 10.0f);
                ImGui::Text("Texture: %s", component.Texture ? "Loaded" : "None");
                // Future: Add DragDrop target for HDR texture here
            });
    }

    template<typename T, typename UIFunction>
    void SceneHierarchyPanel::DrawComponent(const std::string& name, Entity entity, UIFunction uiFunction)
    {
        const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;

        if (entity.HasComponent<T>())
        {
            auto& component = entity.GetComponent<T>();
            ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
            float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;

            ImGui::Separator();

            bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, name.c_str());
            ImGui::PopStyleVar();

            ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
            if (ImGui::Button("-", ImVec2{ lineHeight, lineHeight }))
            {
                ImGui::OpenPopup("ComponentSettings");
            }

            bool removeComponent = false;
            if (ImGui::BeginPopup("ComponentSettings"))
            {
                if (ImGui::MenuItem("Remove Component"))
                    removeComponent = true;

                ImGui::EndPopup();
            }

            if (open)
            {
                uiFunction(component);
                ImGui::TreePop();
            }

            if (removeComponent)
                entity.RemoveComponent<T>();
        }
    }
}