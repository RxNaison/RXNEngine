#include "rxnpch.h"
#include "SceneHierarchyPanel.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

#include "RXNEngine/Renderer/ModelImporter.h"

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
                    if (entity.GetComponent<RelationshipComponent>().ParentHandle == 0)
                        DrawEntityNode(entity);
                });

            ImVec2 availRegion = ImGui::GetContentRegionAvail();
			availRegion.x = glm::max(1.0f, availRegion.x);
			availRegion.y = glm::max(1.0f, availRegion.y);
            ImGui::InvisibleButton("##WindowDropZone", ImVec2(availRegion.x, glm::max(availRegion.y, 20.0f)));
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                {
                    std::string path = (const char*)payload->Data;
                    if (!path.empty() && (path.ends_with(".gltf") || path.ends_with(".glb") || path.ends_with(".obj") || path.ends_with(".fbx")))
                    {
                        ModelImporter::InstantiateToScene(m_Context, path);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                m_SelectedEntity = {};

            if (ImGui::BeginPopupContextItem("EmptySpacePopup", ImGuiPopupFlags_MouseButtonRight))
            {
                if (ImGui::MenuItem("Create New Entity"))
                    m_Context->CreateEntity("New Entity");

                ImGui::EndPopup();
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY"))
                {
                    UUID droppedEntityID = *(UUID*)payload->Data;
                    Entity droppedEntity = m_Context->GetEntityByUUID(droppedEntityID);
                    if (droppedEntity)
                        m_Context->ParentEntity(droppedEntity, {});
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
                m_SelectedEntity = {};

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

    Entity SceneHierarchyPanel::ResolvePickedEntity(Entity entity)
    {
        if (!entity) return {};

        auto& rc = entity.GetComponent<RelationshipComponent>();

        if (rc.ParentHandle != 0)
        {
            Entity parent = m_Context->GetEntityByUUID(rc.ParentHandle);
            if (parent)
            {
                if (m_ExpandedNodes.find((uint64_t)parent.GetUUID()) == m_ExpandedNodes.end())
                {
                    return ResolvePickedEntity(parent);
                }
            }
        }

        return entity;
    }

    void SceneHierarchyPanel::DrawEntityNode(Entity entity)
    {
        auto& tag = entity.GetComponent<TagComponent>().Tag;
        auto& rc = entity.GetComponent<RelationshipComponent>();

        ImGuiTreeNodeFlags flags = ((m_SelectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
        flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

        if (rc.Children.empty())
            flags |= ImGuiTreeNodeFlags_Leaf;

        bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entity.GetUUID(), flags, tag.c_str());

        if (opened)
            m_ExpandedNodes.insert((uint64_t)entity.GetUUID());
        else
            m_ExpandedNodes.erase((uint64_t)entity.GetUUID());

        if (ImGui::BeginDragDropSource())
        {
            UUID entityID = entity.GetUUID();
            ImGui::SetDragDropPayload("SCENE_HIERARCHY_ENTITY", &entityID, sizeof(UUID));
            ImGui::Text("%s", tag.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY"))
            {
                UUID droppedEntityID = *(UUID*)payload->Data;
                Entity droppedEntity = m_Context->GetEntityByUUID(droppedEntityID);
                if (droppedEntity)
                    m_Context->ParentEntity(droppedEntity, entity);
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::IsItemClicked())
        {
            m_SelectedEntity = entity;
        }

        bool entityDeleted = false;
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Delete Entity"))
                entityDeleted = true;

            if (ImGui::MenuItem("Create Child Entity"))
            {
                Entity child = m_Context->CreateEntity("New Child");
                m_Context->ParentEntity(child, entity);
            }

            ImGui::EndPopup();
        }

        if (opened)
        {
            for (UUID childID : rc.Children)
            {
                Entity child = m_Context->GetEntityByUUID(childID);
                if (child)
                    DrawEntityNode(child);
            }
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
            if (!m_SelectedEntity.HasComponent<StaticMeshComponent>())
            {
                if (ImGui::MenuItem("Mesh"))
                {
                    m_SelectedEntity.AddComponent<StaticMeshComponent>();
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
            if (!m_SelectedEntity.HasComponent<RigidbodyComponent>())
            {
                if (ImGui::MenuItem("Rigidbody"))
                {
                    m_SelectedEntity.AddComponent<RigidbodyComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }
            if (!m_SelectedEntity.HasComponent<BoxColliderComponent>())
            {
                if (ImGui::MenuItem("Box Collider"))
                {
                    m_SelectedEntity.AddComponent<BoxColliderComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }
            if (!m_SelectedEntity.HasComponent<SphereColliderComponent>())
            {
                if (ImGui::MenuItem("Sphere Collider"))
                {
                    m_SelectedEntity.AddComponent<SphereColliderComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }
            if (!m_SelectedEntity.HasComponent<CapsuleColliderComponent>())
            {
                if (ImGui::MenuItem("Capsule Collider"))
                {
                    m_SelectedEntity.AddComponent<CapsuleColliderComponent>();
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
        DrawComponent<CameraComponent>("Camera", entity, [&](auto& component)
            {
                auto& camera = component.Camera;
                auto primaryCamera = m_Context->GetPrimaryCameraEntity();
                bool isPrimary = false;

                if (primaryCamera)
                {
                    isPrimary = (entity.GetUUID() == primaryCamera.GetUUID());
                }

                if (ImGui::Checkbox("Primary", &isPrimary))
                {
                    if (isPrimary)
                        m_Context->SetPrimaryCameraEntity(entity);
                }

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


        // --- 4. Mesh Component ---
        DrawComponent<StaticMeshComponent>("Mesh", entity, [](auto& component)
            {
                ImGui::Columns(2);
                ImGui::SetColumnWidth(0, 100.0f);
                ImGui::Text("Mesh Asset");
                ImGui::NextColumn();

                std::string label = component.Mesh ? component.AssetPath.substr(component.AssetPath.find_last_of("/\\") + 1) : "Drop Model Here";

                if (ImGui::Button(label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                {
                    std::string path = FileDialogs::OpenFile("*.obj *fbx *.glb *.gltf");
                    if (!path.empty())
                    {
                        component.AssetPath = path;
                        component.Mesh = AssetManager::GetMesh(path);
                        component.SubmeshIndex = 0;
                        component.MaterialTableOverride = nullptr;
                    }
                }

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        std::string path = (const char*)payload->Data;
                        if (!path.empty())
                        {
                            component.AssetPath = path;
                            component.Mesh = AssetManager::GetMesh(path);
                            component.SubmeshIndex = 0;
                            component.MaterialTableOverride = nullptr;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::Columns(1);

                if (component.Mesh)
                {
                    ImGui::Separator();

                    uint32_t submeshCount = (uint32_t)component.Mesh->GetSubmeshes().size();
                    if (submeshCount > 1)
                    {
                        int submeshIndex = (int)component.SubmeshIndex;
                        if (ImGui::SliderInt("Submesh Index", &submeshIndex, 0, submeshCount - 1))
                        {
                            component.SubmeshIndex = (uint32_t)submeshIndex;
                            component.MaterialTableOverride = nullptr;
                        }

                        ImGui::Text("Node Name: %s", component.Mesh->GetSubmeshes()[component.SubmeshIndex].NodeName.c_str());
                    }

                    ImGui::Separator();
                    ImGui::Text("Material Properties");

                    bool hasOverride = component.MaterialTableOverride != nullptr;
                    if (ImGui::Checkbox("Override Default Material", &hasOverride))
                    {
                        if (hasOverride)
                        {
                            uint32_t matIndex = component.Mesh->GetSubmeshes()[component.SubmeshIndex].MaterialIndex;
                            Ref<Material> defaultMat = component.Mesh->GetMaterials()[matIndex]; // FIXED

                            component.MaterialTableOverride = Material::CreateDefault(defaultMat->GetShader());

                            auto params = defaultMat->GetParameters();
                            component.MaterialTableOverride->SetAlbedoColor(params.AlbedoColor);
                            component.MaterialTableOverride->SetRoughness(params.Roughness);
                            component.MaterialTableOverride->SetMetalness(params.Metalness);
                            component.MaterialTableOverride->SetAO(params.AO);
                            component.MaterialTableOverride->SetEmissiveColor(params.EmissiveColor);

                            component.MaterialTableOverride->SetAlbedoMap(defaultMat->GetAlbedoMap());
                            component.MaterialTableOverride->SetNormalMap(defaultMat->GetNormalMap());
                            component.MaterialTableOverride->SetMetalnessRoughnessMap(defaultMat->GetMetalnessRoughnessMap());
                            component.MaterialTableOverride->SetAOMap(defaultMat->GetAOMap());
                        }
                        else
                        {
                            component.MaterialTableOverride = nullptr;
                        }
                    }

                    uint32_t activeMatIndex = component.Mesh->GetSubmeshes()[component.SubmeshIndex].MaterialIndex;
                    Ref<Material> activeMaterial = component.MaterialTableOverride ?
                        component.MaterialTableOverride :
                        component.Mesh->GetMaterials()[activeMatIndex];

                    if (!hasOverride) ImGui::BeginDisabled();

                    auto params = activeMaterial->GetParameters();

                    if (ImGui::ColorEdit4("Albedo", glm::value_ptr(params.AlbedoColor)))
                        activeMaterial->SetAlbedoColor(params.AlbedoColor);

                    if (ImGui::SliderFloat("Roughness", &params.Roughness, 0.0f, 1.0f))
                        activeMaterial->SetRoughness(params.Roughness);

                    if (ImGui::SliderFloat("Metalness", &params.Metalness, 0.0f, 1.0f))
                        activeMaterial->SetMetalness(params.Metalness);

                    if (ImGui::DragFloat("AO Strength", &params.AO, 0.01f, 0.0f, 1.0f))
                        activeMaterial->SetAO(params.AO);

                    if (ImGui::ColorEdit3("Emissive", glm::value_ptr(params.EmissiveColor)))
                        activeMaterial->SetEmissiveColor(params.EmissiveColor);

                    if (!hasOverride) ImGui::EndDisabled();
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

        // --- 7. Rigidbody ---
        DrawComponent<RigidbodyComponent>("Rigidbody", entity, [](auto& component)
            {
                const char* bodyTypeStrings[] = { "Static", "Dynamic", "Kinematic" };
                const char* currentBodyTypeString = bodyTypeStrings[(int)component.Type];
                if (ImGui::BeginCombo("Body Type", currentBodyTypeString))
                {
                    for (int i = 0; i < 3; i++)
                    {
                        bool isSelected = currentBodyTypeString == bodyTypeStrings[i];
                        if (ImGui::Selectable(bodyTypeStrings[i], isSelected))
                        {
                            currentBodyTypeString = bodyTypeStrings[i];
                            component.Type = (RigidbodyComponent::BodyType)i;
                        }
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::DragFloat("Mass", &component.Mass, 0.1f, 0.0f, 1000.0f);
                ImGui::DragFloat("Linear Drag", &component.LinearDrag, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Angular Drag", &component.AngularDrag, 0.01f, 0.0f, 1.0f);
            });

        // --- 8. Box Collider ---
        DrawComponent<BoxColliderComponent>("Box Collider", entity, [](auto& component)
            {
				DrawVec3Control("HalfExtents", component.HalfExtents, 0.5f);
				DrawVec3Control("Offset", component.Offset);
				ImGui::DragFloat("Static Friction", &component.StaticFriction, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Dynamic Friction", &component.DynamicFriction, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
            });
        // --- 9. Sphere Collider ---
        DrawComponent<SphereColliderComponent>("Sphere Collider", entity, [](auto& component)
            {
                ImGui::DragFloat("Radius", &component.Radius, 0.05f, 0.0f, 100.0f);
                DrawVec3Control("Offset", component.Offset);
                ImGui::DragFloat("Static Friction", &component.StaticFriction, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Dynamic Friction", &component.DynamicFriction, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
            });

        // --- 10. Capsule Collider ---
        DrawComponent<CapsuleColliderComponent>("Capsule Collider", entity, [](auto& component)
            {
                ImGui::DragFloat("Radius", &component.Radius, 0.05f, 0.0f, 100.0f);
                ImGui::DragFloat("Height", &component.Height, 0.05f, 0.0f, 100.0f);
                DrawVec3Control("Offset", component.Offset);
                ImGui::DragFloat("Static Friction", &component.StaticFriction, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Dynamic Friction", &component.DynamicFriction, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
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