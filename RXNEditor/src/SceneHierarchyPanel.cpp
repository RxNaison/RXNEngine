#include "rxnpch.h"
#include "SceneHierarchyPanel.h"
#include "UI.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

#include "RXNEngine/Asset/ModelImporter.h"
#include "RXNEngine/Scripting/ScriptEngine.h"
#include "RXNEngine/Serialization/MaterialSerializer.h"
#include "RXNEngine/Serialization/PrefabSerializer.h"

namespace RXNEditor {

	template<typename T, typename ... Args>
    void ShowAddComponentEntry(const std::string& name, Entity& entity, Args&& ... args)
    {
        if (!entity.HasComponent<T>())
        {
            if (ImGui::MenuItem(name.c_str()))
            {
                entity.AddComponent<T>(std::forward<Args>(args)...);
                ImGui::CloseCurrentPopup();
            }
        }
	}

    SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context)
    {
        SetContext(context);
    }

    void SceneHierarchyPanel::SetContext(const Ref<Scene>& context)
    {
        m_Context = context;
        ClearSelection();
    }

    void SceneHierarchyPanel::SetSelectedEntity(Entity entity)
    {
        m_SelectedEntities.clear();

        if (entity)
            m_SelectedEntities.push_back(entity);

        m_SelectionContextAnchor = entity;
    }

    void SceneHierarchyPanel::ToggleSelection(Entity entity)
    {
        auto it = std::find(m_SelectedEntities.begin(), m_SelectedEntities.end(), entity);
        if (it != m_SelectedEntities.end())
            m_SelectedEntities.erase(it);
        else
            m_SelectedEntities.push_back(entity);
    }

    bool SceneHierarchyPanel::IsEntitySelected(Entity entity) const
    {
        return std::find(m_SelectedEntities.begin(), m_SelectedEntities.end(), entity) != m_SelectedEntities.end();
    }

    void SceneHierarchyPanel::ClearSelection()
    {
        m_SelectedEntities.clear();
        m_SelectionContextAnchor = {};
    }

    void SceneHierarchyPanel::OnImGuiRender()
    {
        m_VisibleNodes.clear();

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
                        if (m_MeshDropCallback)
                            m_MeshDropCallback(path);
                    }
                    else if (path.ends_with(".rxnpb"))
                    {
                        Entity prefabRoot = PrefabSerializer::Deserialize(m_Context, path);
                        if (prefabRoot)
							SetSelectedEntity(prefabRoot);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                ClearSelection();

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

            if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
                ClearSelection();

            if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight))
            {
                if (ImGui::MenuItem("Create New Entity"))
                    m_Context->CreateEntity("New Entity");

                ImGui::EndPopup();
            }
        }
        ImGui::End();

        ImGui::Begin("Properties");
        if (m_SelectedEntities.size() == 1)
        {
            if (m_SelectedEntities[0].IsValid())
                DrawComponents(m_SelectedEntities[0]);
            else
                ClearSelection();
        }
        else if (m_SelectedEntities.size() > 1)
        {
            ImGui::Text("Multiple Entities Selected (%zu)", m_SelectedEntities.size());
            ImGui::Separator();
            if (ImGui::Button("Delete All Selected", ImVec2(ImGui::GetContentRegionAvail().x, 30.0f)))
            {
                for (Entity e : m_SelectedEntities)
                    m_Context->DestroyEntity(e);
                ClearSelection();
            }
        }
        ImGui::End();
    }

    Entity SceneHierarchyPanel::ResolvePickedEntity(Entity entity)
    {
        if (!entity)
            return {};

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

        m_VisibleNodes.push_back(entity);

        bool isSelected = IsEntitySelected(entity);
        ImGuiTreeNodeFlags flags = (isSelected ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
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

        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            if (!ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left))
            {
                if (ImGui::GetIO().KeyShift && m_SelectionContextAnchor)
                {
                    int startIndex = -1, endIndex = -1;

                    for (int i = 0; i < (int)m_VisibleNodes.size(); i++)
                    {
                        if (m_VisibleNodes[i] == m_SelectionContextAnchor)
                            startIndex = i;

                        if (m_VisibleNodes[i] == entity)
                            endIndex = i;
                    }

                    if (startIndex != -1 && endIndex != -1)
                    {
                        if (!ImGui::GetIO().KeyCtrl)
                            m_SelectedEntities.clear();

                        int minIdx = std::min(startIndex, endIndex);
                        int maxIdx = std::max(startIndex, endIndex);

                        for (int i = minIdx; i <= maxIdx; i++)
                        {
                            if (!IsEntitySelected(m_VisibleNodes[i]))
                                m_SelectedEntities.push_back(m_VisibleNodes[i]);
                        }
                    }
                    else
                    {
                        SetSelectedEntity(entity);
                    }
                }
                else if (ImGui::GetIO().KeyCtrl)
                {
                    ToggleSelection(entity);
                    m_SelectionContextAnchor = entity;
                }
                else
                {
                    SetSelectedEntity(entity);
                }
            }
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

            if (ImGui::MenuItem("Save as Prefab..."))
            {
                std::string savePath = FileDialogs::SaveFile("Prefab (*.rxnpb)\0*.rxnpb\0");
                if (!savePath.empty())
                {
                    PrefabSerializer::Serialize(entity, savePath);
                    RXN_CORE_INFO("Saved Prefab to {0}", savePath);
                }
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
            if (isSelected)
                ToggleSelection(entity);
        }
    }

    void SceneHierarchyPanel::DrawComponents(Entity entity)
    {
        uint64_t id = entity.GetComponent<IDComponent>().ID;

        ImGui::TextDisabled(std::to_string(id).c_str());

        if (entity.HasComponent<TagComponent>())
        {
            auto& tag = entity.GetComponent<TagComponent>().Tag;

            char buffer[256];
            memset(buffer, 0, sizeof(buffer));
            strcpy_s(buffer, sizeof(buffer), tag.c_str());

            if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
                tag = std::string(buffer);
        }

        ImGui::SameLine();
        ImGui::PushItemWidth(-1);
        if (ImGui::Button("Add Component"))
            ImGui::OpenPopup("AddComponent");

        if (ImGui::BeginPopup("AddComponent"))
        {
			Entity selectedEntity = GetSelectedEntity();

            ShowAddComponentEntry<CameraComponent>("Camera", selectedEntity);
            ShowAddComponentEntry<StaticMeshComponent>("Mesh", selectedEntity);
            ShowAddComponentEntry<DirectionalLightComponent>("Directional Light", selectedEntity);
            ShowAddComponentEntry<PointLightComponent>("Point Light", selectedEntity);
            ShowAddComponentEntry<SpotLightComponent>("Spot Light", selectedEntity);
            ShowAddComponentEntry<RigidbodyComponent>("Rigidbody", selectedEntity);
            ShowAddComponentEntry<BoxColliderComponent>("Box Collider", selectedEntity);
            ShowAddComponentEntry<SphereColliderComponent>("Sphere Collider", selectedEntity);
            ShowAddComponentEntry<CapsuleColliderComponent>("Capsule Collider", selectedEntity);
            ShowAddComponentEntry<MeshColliderComponent>("Mesh Collider", selectedEntity);
            ShowAddComponentEntry<ScriptComponent>("Script Component", selectedEntity, selectedEntity.GetComponent<TagComponent>().Tag);
            ShowAddComponentEntry<CharacterControllerComponent>("Character Controller", selectedEntity);
            ShowAddComponentEntry<AudioSourceComponent>("Audio Source", selectedEntity);
            ImGui::EndPopup();
        }
        ImGui::PopItemWidth();


        DrawComponent<TransformComponent>("Transform", entity, [&](auto& component)
            {
                UI::DrawVec3Control("Translation", component.Translation);

                glm::vec3 rotation = glm::degrees(component.Rotation);
                UI::DrawVec3Control("Rotation", rotation);
                component.Rotation = glm::radians(rotation);

                UI::DrawVec3Control("Scale", component.Scale, 1.0f);

                if (m_Context->IsSimulating())
                  m_Context->SyncTransformToPhysics(entity);
            });


        DrawComponent<CameraComponent>("Camera", entity, [&](auto& component)
            {
                auto& camera = component.Camera;
                auto primaryCamera = m_Context->GetPrimaryCameraEntity();
                bool isPrimary = false;

                if (primaryCamera)
                    isPrimary = (entity.GetUUID() == primaryCamera.GetUUID());

                if (UI::DrawCheckbox("Primary", isPrimary))
                {
                    if (isPrimary)
                        m_Context->SetPrimaryCameraEntity(entity);
                }

                const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
                int projectionIndex = (int)camera.GetProjectionType();

                if (UI::DrawComboBox("Projection", projectionTypeStrings, 2, projectionIndex))
                    camera.SetProjectionType((SceneCamera::ProjectionMode)projectionIndex);

                if (camera.GetProjectionType() == SceneCamera::ProjectionMode::Perspective)
                {
                    float Fov = glm::degrees(camera.GetPerspectiveFOV());
                    if (UI::DrawFloatControl("FOV", Fov))
                        camera.SetPerspectiveFOV(glm::radians(Fov));

                    float orthoNear = camera.GetPerspectiveNearClip();
                    if (UI::DrawFloatControl("Near", orthoNear))
                        camera.SetPerspectiveNearClip(orthoNear);

                    float orthoFar = camera.GetPerspectiveFarClip();
                    if (UI::DrawFloatControl("Far", orthoFar))
                        camera.SetPerspectiveFarClip(orthoFar);
                }

                if (camera.GetProjectionType() == SceneCamera::ProjectionMode::Orthographic)
                {
                    float orthoSize = camera.GetOrthographicSize();
                    if (UI::DrawFloatControl("Size", orthoSize))
                        camera.SetOrthographicSize(orthoSize);

                    float orthoNear = camera.GetOrthographicNearClip();
                    if (UI::DrawFloatControl("Near", orthoNear))
                        camera.SetOrthographicNearClip(orthoNear);

                    float orthoFar = camera.GetOrthographicFarClip();
                    if (UI::DrawFloatControl("Far", orthoFar))
                        camera.SetOrthographicFarClip(orthoFar);
                }
            });


        DrawComponent<StaticMeshComponent>("Static Mesh", entity, [&](auto& component)
            {
                auto assetManager = Application::Get().GetSubsystem<AssetManager>();

                ImGui::Text("Mesh");
                ImGui::SameLine(100.0f);
                std::string meshLabel = component.Mesh ? component.AssetPath.substr(component.AssetPath.find_last_of("/\\") + 1) : "None";
                ImGui::Button(meshLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        std::string path = (const char*)payload->Data;
                        if (path.ends_with(".gltf") || path.ends_with(".obj"))
                        {
                            component.AssetPath = path;
                            component.Mesh = assetManager->GetMesh(path);
                            component.MaterialTableOverride = nullptr;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (!component.Mesh) return;

                ImGui::Separator();

                ImGui::Text("Material");
                ImGui::SameLine(100.0f);

                std::string matName = component.MaterialAssetPath.empty() ? "Default Material" : component.MaterialAssetPath.substr(component.MaterialAssetPath.find_last_of("/\\") + 1);

                ImGui::Button(matName.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        std::string path = (const char*)payload->Data;
                        if (path.ends_with(".rxnmat"))
                        {
                            component.MaterialTableOverride = assetManager->GetMaterial(path);
                            component.MaterialAssetPath = path;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (component.MaterialTableOverride)
                {
                    if (ImGui::Button("Reset to Default"))
                    {
                        component.MaterialTableOverride = nullptr;
                        component.MaterialAssetPath = "";
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Apply to Entire Model"))
                    {
                        auto& rc = entity.GetComponent<RelationshipComponent>();
                        Entity rootEntity = entity;
                        if (rc.ParentHandle != 0)
                            rootEntity = m_Context->GetEntityByUUID(rc.ParentHandle);

                        for (UUID childID : rootEntity.GetComponent<RelationshipComponent>().Children)
                        {
                            Entity child = m_Context->GetEntityByUUID(childID);
                            if (child && child.HasComponent<StaticMeshComponent>())
                            {
                                auto& childMc = child.GetComponent<StaticMeshComponent>();
                                childMc.MaterialTableOverride = component.MaterialTableOverride;
                                childMc.MaterialAssetPath = component.MaterialAssetPath;
                            }
                        }
                    }
                }
            });

        DrawComponent<DirectionalLightComponent>("Directional Light", entity, [](auto& component)
            {
                UI::DrawColor3Control("Color", component.Color);
                UI::DrawFloatControl("Intensity", component.Intensity, 0.1f, 0.0f, 100.0f);
            });


        DrawComponent<PointLightComponent>("Point Light", entity, [](auto& component)
            {
                UI::DrawColor3Control("Color", component.Color);
                UI::DrawFloatControl("Intensity", component.Intensity, 0.1f, 0.0f, 100.0f);
                UI::DrawFloatControl("Radius", component.Radius, 0.1f, 0.0f, 1000.0f);
                UI::DrawFloatControl("Falloff", component.Falloff, 0.05f, 0.0f, 10.0f);
                UI::DrawCheckbox("Casts Shadows", component.CastsShadows);
            });

        DrawComponent<SpotLightComponent>("Spot Light", entity, [](auto& component)
            {
                UI::DrawColor3Control("Color", component.Color);
                UI::DrawFloatControl("Intensity", component.Intensity, 0.1f, 0.0f, 100.0f);
                UI::DrawFloatControl("Radius", component.Radius, 0.1f, 0.0f, 1000.0f);
                UI::DrawFloatControl("Falloff", component.Falloff, 0.05f, 0.0f, 10.0f);
                UI::DrawFloatControl("Inner Angle", component.InnerAngle, 0.1f, 0.0f, 90.0f);
                UI::DrawFloatControl("Outer Angle", component.OuterAngle, 0.1f, 0.0f, 90.0f);
                UI::DrawCheckbox("Casts Shadows", component.CastsShadows);

                ImGui::Separator();
                ImGui::Text("Cookie Texture");
                ImGui::SameLine(100.0f);

                bool hasCookie = (component.CookieTexture != nullptr || component.CookieVideo != nullptr);
                std::string cookieLabel = hasCookie ? component.CookieAssetPath.substr(component.CookieAssetPath.find_last_of("/\\") + 1) : "None";

                ImGui::Button(cookieLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        std::string path = (const char*)payload->Data;

                        if (path.ends_with(".png") || path.ends_with(".jpg"))
                        {
                            component.CookieAssetPath = path;
                            component.IsVideo = false;
                            component.CookieVideo = nullptr;
                            component.CookieTexture = Application::Get().GetSubsystem<AssetManager>()->GetTexture(path);
                        }
                        else if (path.ends_with(".mpg") || path.ends_with(".mpeg"))
                        {
                            component.CookieAssetPath = path;
                            component.IsVideo = true;
                            component.CookieTexture = nullptr;
                            component.CookieVideo = CreateRef<VideoTexture>(path);
                        }
                    }

                    ImGui::EndDragDropTarget();
                }

                if (hasCookie)
                {
                    UI::DrawFloatControl("Cookie Size", component.CookieSize, 0.05f, 0.1f, 10.0f);

                    if (ImGui::Button("Remove Cookie", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                    {
                        component.CookieAssetPath = "";
                        component.CookieTexture = nullptr;
                        component.CookieVideo = nullptr;
                        component.IsVideo = false;
                    }
                }
            });

        DrawComponent<RigidbodyComponent>("Rigidbody", entity, [](auto& component)
            {
                const char* bodyTypeStrings[] = { "Static", "Dynamic", "Kinematic" };
                int bodyTypeIndex = (int)component.Type;

                if (UI::DrawComboBox("Body Type", bodyTypeStrings, 3, bodyTypeIndex))
                    component.Type = (RigidbodyComponent::BodyType)bodyTypeIndex;

                UI::DrawFloatControl("Mass", component.Mass, 0.1f, 0.0f, 1000.0f);
                UI::DrawFloatControl("Linear Drag", component.LinearDrag, 0.01f, 0.0f, 1.0f);
                UI::DrawFloatControl("Angular Drag", component.AngularDrag, 0.01f, 0.0f, 1.0f);

                UI::DrawCheckbox("Fixed Rotation", component.FixedRotation);

                UI::DrawCheckbox("CCD", component.UseCCD);
                if (component.UseCCD)
                {
                    ImGui::Indent();
                    UI::DrawFloatControl("Velocity Threshold", component.CCDVelocityThreshold, 1.0f, 0.0f, 1000.0f, 130.0f);
                    ImGui::Unindent();
                }
            });

        auto drawPhysicsMaterialWidget = [&](auto& component)
            {
                ImGui::Text("Physics Material");
                ImGui::SameLine(130.0f);
                std::string matName = component.PhysicsMaterialPath.empty() ? "Default" : component.PhysicsMaterialPath.substr(component.PhysicsMaterialPath.find_last_of("/\\") + 1);

                ImGui::Button(matName.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                        std::string path = (const char*)payload->Data;
                        if (path.ends_with(".rxnphys")) {
                            component.PhysicsMaterialPath = path;
                            component.PhysicsMaterialAsset = Application::Get().GetSubsystem<AssetManager>()->GetPhysicsMaterial(path);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (component.PhysicsMaterialAsset) {
                    if (ImGui::Button("Reset Material", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                        component.PhysicsMaterialAsset = nullptr;
                        component.PhysicsMaterialPath = "";
                    }
                }
            };

        DrawComponent<BoxColliderComponent>("Box Collider", entity, [&](auto& component)
            {
                UI::DrawVec3Control("HalfExtents", component.HalfExtents, 0.5f);
                UI::DrawVec3Control("Offset", component.Offset);
                UI::DrawCheckbox("Is Trigger", component.IsTrigger);
                drawPhysicsMaterialWidget(component);
            });
        DrawComponent<SphereColliderComponent>("Sphere Collider", entity, [&](auto& component)
            {
                UI::DrawFloatControl("Radius", component.Radius, 0.05f, 0.0f, 100.0f);
                UI::DrawVec3Control("Offset", component.Offset);
                UI::DrawCheckbox("Is Trigger", component.IsTrigger);
                drawPhysicsMaterialWidget(component);
            });

        DrawComponent<CapsuleColliderComponent>("Capsule Collider", entity, [&](auto& component)
            {
                UI::DrawFloatControl("Radius", component.Radius, 0.05f, 0.0f, 100.0f);
                UI::DrawFloatControl("Height", component.Height, 0.05f, 0.0f, 100.0f);
                UI::DrawVec3Control("Offset", component.Offset);
                UI::DrawCheckbox("Is Trigger", component.IsTrigger);
                drawPhysicsMaterialWidget(component);
            });

        DrawComponent<MeshColliderComponent>("Mesh Collider", entity, [&](auto& component)
            {
                UI::DrawCheckbox("Is Convex", component.IsConvex);
                UI::DrawCheckbox("Is Trigger", component.IsTrigger);
                drawPhysicsMaterialWidget(component);
            });

        DrawComponent<CharacterControllerComponent>("Character Controller", entity, [](auto& component)
            {
                UI::DrawFloatControl("Slope Limit (Deg)", component.SlopeLimitDegrees, 1.0f, 0.0f, 90.0f);
                UI::DrawFloatControl("Step Offset", component.StepOffset, 0.05f, 0.0f, 5.0f);
                UI::DrawFloatControl("Radius", component.Radius, 0.05f, 0.0f, 10.0f);
                UI::DrawFloatControl("Height", component.Height, 0.05f, 0.0f, 10.0f);
            });

        DrawComponent<ScriptComponent>("Script", entity, [&](auto& component)
            {
                char buffer[256];
                memset(buffer, 0, sizeof(buffer));
                strncpy(buffer, component.ClassName.c_str(), sizeof(buffer));

                ImGui::PushID("Class");
                ImGui::Columns(2);
                ImGui::SetColumnWidth(0, 100.0f);
                ImGui::Text("Class");
                ImGui::NextColumn();

                ImGui::PushItemWidth(-1);
                if (ImGui::InputText("##Class", buffer, sizeof(buffer)))
                    component.ClassName = std::string(buffer);

                ImGui::PopItemWidth();
                ImGui::Columns(1);
                ImGui::PopID();

                auto scriptSys = Application::Get().GetSubsystem<ScriptEngine>();
                bool classExists = scriptSys->EntityClassExists(component.ClassName);

                if (!classExists && !component.ClassName.empty())
                    ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "Warning: Class does not exist in loaded Assembly!");

                if (classExists)
                {
                    bool isRunning = m_Context->IsRunning();
                    Ref<ScriptInstance> instance = nullptr;

                    if (isRunning)
                        instance = scriptSys->GetEntityScriptInstance(entity.GetUUID());

                    const auto& fields = scriptSys->GetClassFields(component.ClassName);
                    for (const auto& field : fields)
                    {
                        if (component.FieldInstances.find(field.Name) == component.FieldInstances.end())
                        {
                            component.FieldInstances[field.Name] = { (uint32_t)field.Type, {0} };

                            if (classExists && !isRunning)
                            {
                                Ref<ScriptInstance> tempInstance = CreateRef<ScriptInstance>(scriptSys.get(), entity, component.ClassName);
                                tempInstance->GetFieldValueInternal(field.Name, component.FieldInstances[field.Name].Data.data());
                            }
                        }

                        auto& fieldInst = component.FieldInstances[field.Name];
                        uint8_t* dataPtr = fieldInst.Data.data();

                        if (isRunning && instance)
                            instance->GetFieldValueInternal(field.Name, dataPtr);

                        switch (field.Type)
                        {
                            case ScriptFieldType::Float:
                            {
                                float data = *(float*)dataPtr;
                                if (UI::DrawFloatControl(field.Name.c_str(), data, 0.1f))
                                {
                                    memcpy(dataPtr, &data, sizeof(float));
                                    if (isRunning && instance)
                                        instance->SetFieldValueInternal(field.Name, dataPtr);
                                }
                                break;
                            }
                            case ScriptFieldType::Bool:
                            {
                                bool data = *(bool*)dataPtr;
                                if (UI::DrawCheckbox(field.Name.c_str(), data))
                                {
                                    memcpy(dataPtr, &data, sizeof(bool));
                                    if (isRunning && instance)
                                        instance->SetFieldValueInternal(field.Name, dataPtr);
                                }
                                break;
                            }
                            case ScriptFieldType::Int:
                            {
                                int data = *(int*)dataPtr;
                                if (UI::DrawIntControl(field.Name.c_str(), data))
                                {
                                    memcpy(dataPtr, &data, sizeof(int));
                                    if (isRunning && instance)
                                        instance->SetFieldValueInternal(field.Name, dataPtr);
                                }
                                break;
                            }
                            case ScriptFieldType::Vector2:
                            {
                                glm::vec2 data = *(glm::vec2*)dataPtr;
                                if (UI::DrawVec2Control(field.Name.c_str(), data, 0.1f))
                                {
                                    memcpy(dataPtr, &data, sizeof(glm::vec2));
                                    if (isRunning && instance)
                                        instance->SetFieldValueInternal(field.Name, dataPtr);
                                }
                                break;
                            }
                            case ScriptFieldType::Vector3:
                            {
                                glm::vec3 data = *(glm::vec3*)dataPtr;
                                if (UI::DrawVec3Control(field.Name.c_str(), data, 0.1f))
                                {
                                    memcpy(dataPtr, &data, sizeof(glm::vec3));
                                    if (isRunning && instance)
                                        instance->SetFieldValueInternal(field.Name, dataPtr);
                                }
                                break;
                            }
                            case ScriptFieldType::Vector4:
                            {
                                glm::vec4 data = *(glm::vec4*)dataPtr;
                                bool isColor = field.Name.find("Color") != std::string::npos;
                                bool changed = isColor ? UI::DrawColor4Control(field.Name.c_str(), data) : UI::DrawColor4Control(field.Name.c_str(), data, 0.1f);
                                if (changed)
                                {
                                    memcpy(dataPtr, &data, sizeof(glm::vec4));
                                    if (isRunning && instance)
                                        instance->SetFieldValueInternal(field.Name, dataPtr);
                                }
                                break;
                            }
                            case ScriptFieldType::Entity:
                            {
                                ImGui::Text("%s", field.Name.c_str());
                                ImGui::SameLine();

                                uint64_t targetID = *(uint64_t*)dataPtr;
                                std::string buttonText = targetID == 0 ? "None (Entity)" : std::to_string(targetID);

                                if (targetID != 0)
                                {
                                    Entity targetEntity = m_Context->GetEntityByUUID(targetID);
                                    if (targetEntity)
                                        buttonText = targetEntity.GetComponent<TagComponent>().Tag;
                                    else 
                                        buttonText = "Invalid Entity";
                                }

                                std::string buttonID = buttonText + "##" + field.Name;
                                ImGui::Button(buttonID.c_str(), ImVec2(100.0f, 0.0f));

                                if (ImGui::BeginDragDropTarget()) 
                                {
                                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY")) 
                                    {
                                        targetID = *(UUID*)payload->Data;
                                        memcpy(dataPtr, &targetID, sizeof(uint64_t));
                                        if (isRunning && instance)
                                            instance->SetFieldValueInternal(field.Name, dataPtr);
                                    }
                                    ImGui::EndDragDropTarget();
                                }
                                break;
                            }
                        }
                    }
                }
            });

            DrawComponent<AudioSourceComponent>("Audio Source", entity, [](auto& component)
                {
                    ImGui::Text("Audio Clip");
                    ImGui::SameLine(100.0f);
                    std::string clipName = component.AudioClipPath.empty() ? "None" : component.AudioClipPath.substr(component.AudioClipPath.find_last_of("/\\") + 1);

                    ImGui::Button(clipName.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                        {
                            std::string path = (const char*)payload->Data;
                            if (path.ends_with(".wav") || path.ends_with(".mp3") || path.ends_with(".flac"))
                                component.AudioClipPath = path;
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (!component.AudioClipPath.empty())
                    {
                        if (ImGui::Button("Clear Audio", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                            component.AudioClipPath = "";
                    }

                    ImGui::Separator();
                    UI::DrawCheckbox("Play On Awake", component.PlayOnAwake);
                    UI::DrawCheckbox("Looping", component.Looping);
                    UI::DrawFloatControl("Volume", component.Volume, 0.05f, 0.0f, 10.0f);
                    UI::DrawFloatControl("Min Distance", component.MinDistance, 0.5f, 0.0f, 1000.0f);
                    UI::DrawFloatControl("Max Distance", component.MaxDistance, 0.5f, 0.0f, 1000.0f);
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
                ImGui::OpenPopup("ComponentSettings");

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