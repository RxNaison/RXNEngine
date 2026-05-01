#include "rxnpch.h"
#include "SceneHierarchyPanel.h"

#include "UI.h"
#include "RXNEngine/Asset/ModelImporter.h"
#include "RXNEngine/Scripting/ScriptEngine.h"
#include "RXNEngine/Serialization/MaterialSerializer.h"
#include "RXNEngine/Serialization/PrefabSerializer.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace RXNEditor {

    template<typename T, typename ... Args>
    void ShowAddComponentEntry(const std::string& name, Entity& entity, RXNEngine::Ref<RXNEngine::Scene> scene, Args&& ... args)
    {
        if (!entity.HasComponent<T>())
        {
            if (ImGui::MenuItem(name.c_str()))
            {
                T newComponent(std::forward<Args>(args)...);
                CommandHistory::AddAndExecute(CreateScope<AddComponentCommand<T>>(scene, entity.GetUUID(), newComponent));
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
                        {
                            SetSelectedEntity(prefabRoot);
                            CommandHistory::Push(CreateScope<SpawnEntityTreeCommand>(m_Context, prefabRoot));
                        }
                    }
                }

                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY"))
                {
                    UUID droppedEntityID = *(UUID*)payload->Data;
                    Entity droppedEntity = m_Context->GetEntityByUUID(droppedEntityID);
                    if (droppedEntity)
                    {
                        UUID oldParent = droppedEntity.GetComponent<RelationshipComponent>().ParentHandle;
                        CommandHistory::AddAndExecute(CreateScope<ChangeParentCommand>(m_Context, droppedEntityID, UUID::Null, oldParent));
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                ClearSelection();

            if (ImGui::BeginPopupContextItem("EmptySpacePopup", ImGuiPopupFlags_MouseButtonRight) ||
                ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight))
            {
                if (ImGui::MenuItem("Create New Entity"))
                {
                    Entity newEntity = m_Context->CreateEntity("New Entity");
                    CommandHistory::Push(CreateScope<CreateEntityCommand>(m_Context, newEntity.GetUUID(), newEntity.GetName()));
                }
                ImGui::EndPopup();
            }

            if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
                ClearSelection();
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
                CommandHistory::AddAndExecute(CreateScope<DeleteEntitiesCommand>(m_Context, m_SelectedEntities));
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
            if (parent && m_ExpandedNodes.find((uint64_t)parent.GetUUID()) == m_ExpandedNodes.end())
                return ResolvePickedEntity(parent);
        }
        return entity;
    }

    void SceneHierarchyPanel::DrawEntityNode(Entity entity)
    {
        auto& tag = entity.GetComponent<TagComponent>().Tag;
        auto& rc = entity.GetComponent<RelationshipComponent>();

        m_VisibleNodes.push_back(entity);

        bool isSelected = IsEntitySelected(entity);
        ImGuiTreeNodeFlags flags = (isSelected ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

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
                {
                    UUID oldParent = droppedEntity.GetComponent<RelationshipComponent>().ParentHandle;
                    CommandHistory::AddAndExecute(CreateScope<ChangeParentCommand>(m_Context, droppedEntityID, entity.GetUUID(), oldParent));
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left))
        {
            if (ImGui::GetIO().KeyShift && m_SelectionContextAnchor)
            {
                int startIndex = -1;
                int endIndex = -1;

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

        bool entityDeleted = false;

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Delete Entity"))
                entityDeleted = true;

            if (ImGui::MenuItem("Create Child Entity"))
            {
                Entity child = m_Context->CreateEntity("New Child");
                m_Context->ParentEntity(child, entity);
                CommandHistory::Push(CreateScope<CreateEntityCommand>(m_Context, child.GetUUID(), child.GetName(), entity.GetUUID()));
            }

            if (ImGui::MenuItem("Save as Prefab..."))
            {
                std::string savePath = FileDialogs::SaveFile("Prefab (*.rxnpb)\0*.rxnpb\0");
                if (!savePath.empty())
                    PrefabSerializer::Serialize(entity, savePath);
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
            CommandHistory::AddAndExecute(CreateScope<DeleteEntitiesCommand>(m_Context, std::vector<Entity>{entity}));
            if (isSelected)
                ClearSelection();
        }
    }

    void SceneHierarchyPanel::DrawComponents(Entity entity)
    {
        uint64_t id = entity.GetComponent<IDComponent>().ID;
        ImGui::TextDisabled(std::to_string(id).c_str());

        if (entity.HasComponent<TagComponent>())
        {
            auto& tagComp = entity.GetComponent<TagComponent>();
            m_TagTracker.BeginEdit(tagComp);

            char buffer[256];
            memset(buffer, 0, sizeof(buffer));
            strcpy_s(buffer, sizeof(buffer), tagComp.Tag.c_str());

            bool modified = ImGui::InputText("##Tag", buffer, sizeof(buffer));
            if (modified)
                tagComp.Tag = std::string(buffer);

            m_TagTracker.EndEdit(m_Context, entity, tagComp, modified);
        }

        ImGui::SameLine();
        ImGui::PushItemWidth(-1);

        if (ImGui::Button("Add Component"))
            ImGui::OpenPopup("AddComponent");

        if (ImGui::BeginPopup("AddComponent"))
        {
            Entity selectedEntity = GetSelectedEntity();
            ShowAddComponentEntry<CameraComponent>("Camera", selectedEntity, m_Context);
            ShowAddComponentEntry<StaticMeshComponent>("Mesh", selectedEntity, m_Context);
            ShowAddComponentEntry<DirectionalLightComponent>("Directional Light", selectedEntity, m_Context);
            ShowAddComponentEntry<PointLightComponent>("Point Light", selectedEntity, m_Context);
            ShowAddComponentEntry<SpotLightComponent>("Spot Light", selectedEntity, m_Context);
            ShowAddComponentEntry<RigidbodyComponent>("Rigidbody", selectedEntity, m_Context);
            ShowAddComponentEntry<BoxColliderComponent>("Box Collider", selectedEntity, m_Context);
            ShowAddComponentEntry<SphereColliderComponent>("Sphere Collider", selectedEntity, m_Context);
            ShowAddComponentEntry<CapsuleColliderComponent>("Capsule Collider", selectedEntity, m_Context);
            ShowAddComponentEntry<MeshColliderComponent>("Mesh Collider", selectedEntity, m_Context);
            ShowAddComponentEntry<ScriptComponent>("Script Component", selectedEntity, m_Context, selectedEntity.GetComponent<TagComponent>().Tag);
            ShowAddComponentEntry<CharacterControllerComponent>("Character Controller", selectedEntity, m_Context);
            ShowAddComponentEntry<AudioSourceComponent>("Audio Source", selectedEntity, m_Context);
            ImGui::EndPopup();
        }
        ImGui::PopItemWidth();

        DrawComponent<TransformComponent>("Transform", entity, [&](auto& component)
            {
                m_TransformTracker.BeginEdit(component);
                bool m = false;

                m |= UI::DrawVec3Control("Translation", component.Translation);

                glm::vec3 rot = glm::degrees(component.Rotation);
                m |= UI::DrawVec3Control("Rotation", rot);
                component.Rotation = glm::radians(rot);

                m |= UI::DrawVec3Control("Scale", component.Scale, 1.0f);

                m_TransformTracker.EndEdit(m_Context, entity, component, m);
            });

        DrawComponent<CameraComponent>("Camera", entity, [&](auto& component)
            {
                m_CameraTracker.BeginEdit(component);
                bool m = false;

                auto& camera = component.Camera;
                Entity primaryEntity = m_Context->GetPrimaryCameraEntity();
                bool isPrimary = false;

                if (primaryEntity)
                    isPrimary = (primaryEntity.GetUUID() == entity.GetUUID());

                if (UI::DrawCheckbox("Primary", isPrimary))
                {
                    if (isPrimary)
                        m_Context->SetPrimaryCameraEntity(entity);
                }

                const char* projStrings[] = { "Perspective", "Orthographic" };
                int projIdx = (int)camera.GetProjectionType();

                if (UI::DrawComboBox("Projection", projStrings, 2, projIdx))
                {
                    camera.SetProjectionType((SceneCamera::ProjectionMode)projIdx);
                    m = true;
                }

                if (camera.GetProjectionType() == SceneCamera::ProjectionMode::Perspective)
                {
                    float fov = glm::degrees(camera.GetPerspectiveFOV());
                    if (UI::DrawFloatControl("FOV", fov))
                    {
                        camera.SetPerspectiveFOV(glm::radians(fov));
                        m = true;
                    }

                    float nearC = camera.GetPerspectiveNearClip();
                    if (UI::DrawFloatControl("Near", nearC))
                    {
                        camera.SetPerspectiveNearClip(nearC);
                        m = true;
                    }

                    float farC = camera.GetPerspectiveFarClip();
                    if (UI::DrawFloatControl("Far", farC))
                    {
                        camera.SetPerspectiveFarClip(farC);
                        m = true;
                    }
                }
                else
                {
                    float size = camera.GetOrthographicSize();
                    if (UI::DrawFloatControl("Size", size))
                    {
                        camera.SetOrthographicSize(size);
                        m = true;
                    }

                    float nearC = camera.GetOrthographicNearClip();
                    if (UI::DrawFloatControl("Near", nearC))
                    {
                        camera.SetOrthographicNearClip(nearC);
                        m = true;
                    }

                    float farC = camera.GetOrthographicFarClip();
                    if (UI::DrawFloatControl("Far", farC))
                    {
                        camera.SetOrthographicFarClip(farC);
                        m = true;
                    }
                }

                m_CameraTracker.EndEdit(m_Context, entity, component, m);
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
                    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        std::string path = (const char*)p->Data;
                        if (path.ends_with(".gltf") || path.ends_with(".obj"))
                        {
                            StaticMeshComponent oldState = component;

                            component.AssetPath = path;
                            component.Mesh = assetManager->GetMesh(path);
                            component.MaterialTableOverride = nullptr;

                            CommandHistory::Push(CreateScope<ChangeComponentCommand<StaticMeshComponent>>(m_Context, entity.GetUUID(), oldState, component));
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (!component.Mesh)
                    return;

                ImGui::Separator();
                ImGui::Text("Material");
                ImGui::SameLine(100.0f);

                std::string matName = component.MaterialAssetPath.empty() ? "Default Material" : component.MaterialAssetPath.substr(component.MaterialAssetPath.find_last_of("/\\") + 1);
                ImGui::Button(matName.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        std::string path = (const char*)p->Data;
                        if (path.ends_with(".rxnmat"))
                        {
                            StaticMeshComponent oldState = component;

                            component.MaterialTableOverride = assetManager->GetMaterial(path);
                            component.MaterialAssetPath = path;

                            CommandHistory::Push(CreateScope<ChangeComponentCommand<StaticMeshComponent>>(m_Context, entity.GetUUID(), oldState, component));
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (component.MaterialTableOverride)
                {
                    if (ImGui::Button("Reset to Default"))
                    {
                        StaticMeshComponent oldState = component;

                        component.MaterialTableOverride = nullptr;
                        component.MaterialAssetPath = "";

                        CommandHistory::Push(CreateScope<ChangeComponentCommand<StaticMeshComponent>>(m_Context, entity.GetUUID(), oldState, component));
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
                                StaticMeshComponent oldChildState = childMc;

                                childMc.MaterialTableOverride = component.MaterialTableOverride;
                                childMc.MaterialAssetPath = component.MaterialAssetPath;

                                CommandHistory::Push(CreateScope<ChangeComponentCommand<StaticMeshComponent>>(m_Context, childID, oldChildState, childMc));
                            }
                        }
                    }
                }
            });

        DrawComponent<DirectionalLightComponent>("Directional Light", entity, [&](auto& component)
            {
                m_DirLightTracker.BeginEdit(component);
                bool m = false;

                m |= UI::DrawColor3Control("Color", component.Color);
                m |= UI::DrawFloatControl("Intensity", component.Intensity, 0.1f, 0.0f, 100.0f);

                m_DirLightTracker.EndEdit(m_Context, entity, component, m);
            });

        DrawComponent<PointLightComponent>("Point Light", entity, [&](auto& component)
            {
                m_PointLightTracker.BeginEdit(component);
                bool m = false;

                m |= UI::DrawColor3Control("Color", component.Color);
                m |= UI::DrawFloatControl("Intensity", component.Intensity, 0.1f, 0.0f, 100.0f);
                m |= UI::DrawFloatControl("Radius", component.Radius, 0.1f, 0.0f, 1000.0f);
                m |= UI::DrawFloatControl("Falloff", component.Falloff, 0.05f, 0.0f, 10.0f);
                m |= UI::DrawCheckbox("Casts Shadows", component.CastsShadows);

                m_PointLightTracker.EndEdit(m_Context, entity, component, m);
            });

        DrawComponent<SpotLightComponent>("Spot Light", entity, [&](auto& component)
            {
                m_SpotLightTracker.BeginEdit(component);
                bool m = false;

                m |= UI::DrawColor3Control("Color", component.Color);
                m |= UI::DrawFloatControl("Intensity", component.Intensity, 0.1f, 0.0f, 100.0f);
                m |= UI::DrawFloatControl("Radius", component.Radius, 0.1f, 0.0f, 1000.0f);
                m |= UI::DrawFloatControl("Falloff", component.Falloff, 0.05f, 0.0f, 10.0f);
                m |= UI::DrawFloatControl("Inner Angle", component.InnerAngle, 0.1f, 0.0f, 90.0f);
                m |= UI::DrawFloatControl("Outer Angle", component.OuterAngle, 0.1f, 0.0f, 90.0f);
                m |= UI::DrawCheckbox("Casts Shadows", component.CastsShadows);
                m |= UI::DrawFloatControl("Cookie Size", component.CookieSize, 0.05f, 0.1f, 10.0f);

                m_SpotLightTracker.EndEdit(m_Context, entity, component, m);

                ImGui::Separator();
                ImGui::Text("Cookie");
                ImGui::SameLine(100.0f);

                bool hasCookie = (component.CookieTexture != nullptr || component.CookieVideo != nullptr);
                std::string cookieLabel = hasCookie ? component.CookieAssetPath.substr(component.CookieAssetPath.find_last_of("/\\") + 1) : "None";
                ImGui::Button(cookieLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        std::string path = (const char*)p->Data;
                        SpotLightComponent oldState = component;

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

                        CommandHistory::Push(CreateScope<ChangeComponentCommand<SpotLightComponent>>(m_Context, entity.GetUUID(), oldState, component));
                    }
                    ImGui::EndDragDropTarget();
                }

                if (hasCookie)
                {
                    if (ImGui::Button("Remove Cookie", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                    {
                        SpotLightComponent oldState = component;

                        component.CookieAssetPath = "";
                        component.CookieTexture = nullptr;
                        component.CookieVideo = nullptr;
                        component.IsVideo = false;

                        CommandHistory::Push(CreateScope<ChangeComponentCommand<SpotLightComponent>>(m_Context, entity.GetUUID(), oldState, component));
                    }
                }
            });

        DrawComponent<RigidbodyComponent>("Rigidbody", entity, [&](auto& component)
            {
                m_RigidbodyTracker.BeginEdit(component);
                bool m = false;

                const char* types[] = { "Static", "Dynamic", "Kinematic" };
                int typeIdx = (int)component.Type;

                if (UI::DrawComboBox("Body Type", types, 3, typeIdx))
                {
                    component.Type = (RigidbodyComponent::BodyType)typeIdx;
                    m = true;
                }

                m |= UI::DrawFloatControl("Mass", component.Mass, 0.1f, 0.0f, 1000.0f);
                m |= UI::DrawFloatControl("Linear Drag", component.LinearDrag, 0.01f, 0.0f, 1.0f);
                m |= UI::DrawFloatControl("Angular Drag", component.AngularDrag, 0.01f, 0.0f, 1.0f);
                m |= UI::DrawCheckbox("Fixed Rotation", component.FixedRotation);
                m |= UI::DrawCheckbox("CCD", component.UseCCD);

                if (component.UseCCD)
                {
                    ImGui::Indent();
                    m |= UI::DrawFloatControl("CCD Thresh", component.CCDVelocityThreshold, 1.0f, 0.0f, 1000.0f, 130.0f);
                    ImGui::Unindent();
                }

                m_RigidbodyTracker.EndEdit(m_Context, entity, component, m);
            });

        auto drawPhysMat = [&](auto& component)
            {
                ImGui::Text("Phys Material");
                ImGui::SameLine(130.0f);

                std::string matName = component.PhysicsMaterialPath.empty() ? "Default" : component.PhysicsMaterialPath.substr(component.PhysicsMaterialPath.find_last_of("/\\") + 1);
                ImGui::Button(matName.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        std::string path = (const char*)p->Data;
                        if (path.ends_with(".rxnphys"))
                        {
                            auto oldState = component;

                            component.PhysicsMaterialPath = path;
                            component.PhysicsMaterialAsset = Application::Get().GetSubsystem<AssetManager>()->GetPhysicsMaterial(path);

                            CommandHistory::Push(CreateScope<ChangeComponentCommand<std::decay_t<decltype(component)>>>(m_Context, entity.GetUUID(), oldState, component));
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (component.PhysicsMaterialAsset)
                {
                    if (ImGui::Button("Reset Material", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                    {
                        auto oldState = component;

                        component.PhysicsMaterialAsset = nullptr;
                        component.PhysicsMaterialPath = "";

                        CommandHistory::Push(CreateScope<ChangeComponentCommand<std::decay_t<decltype(component)>>>(m_Context, entity.GetUUID(), oldState, component));
                    }
                }
            };

        DrawComponent<BoxColliderComponent>("Box Collider", entity, [&](auto& component)
            {
                m_BoxColliderTracker.BeginEdit(component);
                bool m = false;

                m |= UI::DrawVec3Control("HalfExtents", component.HalfExtents, 0.5f);
                m |= UI::DrawVec3Control("Offset", component.Offset);
                m |= UI::DrawCheckbox("Is Trigger", component.IsTrigger);

                m_BoxColliderTracker.EndEdit(m_Context, entity, component, m);
                drawPhysMat(component);
            });

        DrawComponent<SphereColliderComponent>("Sphere Collider", entity, [&](auto& component)
            {
                m_SphereColliderTracker.BeginEdit(component);
                bool m = false;

                m |= UI::DrawFloatControl("Radius", component.Radius, 0.05f, 0.0f, 100.0f);
                m |= UI::DrawVec3Control("Offset", component.Offset);
                m |= UI::DrawCheckbox("Is Trigger", component.IsTrigger);

                m_SphereColliderTracker.EndEdit(m_Context, entity, component, m);
                drawPhysMat(component);
            });

        DrawComponent<CapsuleColliderComponent>("Capsule Collider", entity, [&](auto& component)
            {
                m_CapsuleColliderTracker.BeginEdit(component);
                bool m = false;

                m |= UI::DrawFloatControl("Radius", component.Radius, 0.05f, 0.0f, 100.0f);
                m |= UI::DrawFloatControl("Height", component.Height, 0.05f, 0.0f, 100.0f);
                m |= UI::DrawVec3Control("Offset", component.Offset);
                m |= UI::DrawCheckbox("Is Trigger", component.IsTrigger);

                m_CapsuleColliderTracker.EndEdit(m_Context, entity, component, m);
                drawPhysMat(component);
            });

        DrawComponent<MeshColliderComponent>("Mesh Collider", entity, [&](auto& component)
            {
                m_MeshColliderTracker.BeginEdit(component);
                bool m = false;

                m |= UI::DrawCheckbox("Is Convex", component.IsConvex);
                m |= UI::DrawCheckbox("Is Trigger", component.IsTrigger);

                m_MeshColliderTracker.EndEdit(m_Context, entity, component, m);
                drawPhysMat(component);
            });

        DrawComponent<CharacterControllerComponent>("Character Controller", entity, [&](auto& component)
            {
                m_CCTTracker.BeginEdit(component);
                bool m = false;

                m |= UI::DrawFloatControl("Slope Limit", component.SlopeLimitDegrees, 1.0f, 0.0f, 90.0f);
                m |= UI::DrawFloatControl("Step Offset", component.StepOffset, 0.05f, 0.0f, 5.0f);
                m |= UI::DrawFloatControl("Radius", component.Radius, 0.05f, 0.0f, 10.0f);
                m |= UI::DrawFloatControl("Height", component.Height, 0.05f, 0.0f, 10.0f);

                m_CCTTracker.EndEdit(m_Context, entity, component, m);
            });

        DrawComponent<ScriptComponent>("Script", entity, [&](auto& component)
            {
                m_ScriptTracker.BeginEdit(component);
                bool m = false;

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
                {
                    component.ClassName = std::string(buffer);
                    m = true;
                }

                ImGui::PopItemWidth();
                ImGui::Columns(1);
                ImGui::PopID();

                auto scriptSys = Application::Get().GetSubsystem<ScriptEngine>();
                bool exists = scriptSys->EntityClassExists(component.ClassName);

                if (!exists && !component.ClassName.empty())
                {
                    ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "Warning: Class not found!");
                }

                if (exists)
                {
                    bool isRunning = m_Context->IsRunning();
                    Ref<ScriptInstance> inst = isRunning ? scriptSys->GetEntityScriptInstance(entity.GetUUID()) : nullptr;

                    for (const auto& field : scriptSys->GetClassFields(component.ClassName))
                    {
                        if (component.FieldInstances.find(field.Name) == component.FieldInstances.end())
                        {
                            component.FieldInstances[field.Name] = { (uint32_t)field.Type, {0} };
                            if (!isRunning)
                            {
                                auto temp = CreateRef<ScriptInstance>(scriptSys.get(), entity, component.ClassName);
                                temp->GetFieldValueInternal(field.Name, component.FieldInstances[field.Name].Data.data());
                            }
                        }

                        auto& fInst = component.FieldInstances[field.Name];
                        uint8_t* dPtr = fInst.Data.data();

                        if (isRunning && inst)
                        {
                            inst->GetFieldValueInternal(field.Name, dPtr);
                        }

                        switch (field.Type)
                        {
                        case ScriptFieldType::Float:
                        {
                            float d = *(float*)dPtr;
                            if (UI::DrawFloatControl(field.Name.c_str(), d, 0.1f))
                            {
                                memcpy(dPtr, &d, sizeof(float));
                                if (isRunning && inst)
                                    inst->SetFieldValueInternal(field.Name, dPtr);
                                m = true;
                            }
                            break;
                        }
                        case ScriptFieldType::Bool:
                        {
                            bool d = *(bool*)dPtr;
                            if (UI::DrawCheckbox(field.Name.c_str(), d))
                            {
                                memcpy(dPtr, &d, sizeof(bool));
                                if (isRunning && inst)
                                    inst->SetFieldValueInternal(field.Name, dPtr);
                                m = true;
                            }
                            break;
                        }
                        case ScriptFieldType::Int:
                        {
                            int d = *(int*)dPtr;
                            if (UI::DrawIntControl(field.Name.c_str(), d))
                            {
                                memcpy(dPtr, &d, sizeof(int));
                                if (isRunning && inst)
                                    inst->SetFieldValueInternal(field.Name, dPtr);
                                m = true;
                            }
                            break;
                        }
                        case ScriptFieldType::Vector2:
                        {
                            glm::vec2 d = *(glm::vec2*)dPtr;
                            if (UI::DrawVec2Control(field.Name.c_str(), d, 0.1f))
                            {
                                memcpy(dPtr, &d, sizeof(glm::vec2));
                                if (isRunning && inst)
                                    inst->SetFieldValueInternal(field.Name, dPtr);
                                m = true;
                            }
                            break;
                        }
                        case ScriptFieldType::Vector3:
                        {
                            glm::vec3 d = *(glm::vec3*)dPtr;
                            if (UI::DrawVec3Control(field.Name.c_str(), d, 0.1f))
                            {
                                memcpy(dPtr, &d, sizeof(glm::vec3));
                                if (isRunning && inst)
                                    inst->SetFieldValueInternal(field.Name, dPtr);
                                m = true;
                            }
                            break;
                        }
                        case ScriptFieldType::Vector4:
                        {
                            glm::vec4 d = *(glm::vec4*)dPtr;
                            bool isCol = field.Name.find("Color") != std::string::npos;
                            bool c = isCol ? UI::DrawColor4Control(field.Name.c_str(), d) : UI::DrawColor4Control(field.Name.c_str(), d, 0.1f);

                            if (c)
                            {
                                memcpy(dPtr, &d, sizeof(glm::vec4));
                                if (isRunning && inst)
                                    inst->SetFieldValueInternal(field.Name, dPtr);
                                m = true;
                            }
                            break;
                        }
                        case ScriptFieldType::Entity:
                        {
                            ImGui::Text("%s", field.Name.c_str());
                            ImGui::SameLine();

                            uint64_t tID = *(uint64_t*)dPtr;
                            Entity tEnt = m_Context->GetEntityByUUID(tID);
                            std::string bTxt = tID == 0 ? "None (Entity)" : (tEnt ? tEnt.GetComponent<TagComponent>().Tag : "Invalid Entity");

                            ImGui::Button((bTxt + "##" + field.Name).c_str(), ImVec2(100.0f, 0.0f));

                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY"))
                                {
                                    tID = *(UUID*)p->Data;
                                    memcpy(dPtr, &tID, sizeof(uint64_t));
                                    if (isRunning && inst)
                                        inst->SetFieldValueInternal(field.Name, dPtr);
                                    m = true;
                                }
                                ImGui::EndDragDropTarget();
                            }
                            break;
                        }
                        }
                    }
                }
                m_ScriptTracker.EndEdit(m_Context, entity, component, m);
            });

        DrawComponent<AudioSourceComponent>("Audio Source", entity, [&](auto& component)
            {
                m_AudioTracker.BeginEdit(component);
                bool m = false;

                ImGui::Text("Audio Clip");
                ImGui::SameLine(100.0f);

                std::string clipName = component.AudioClipPath.empty() ? "None" : component.AudioClipPath.substr(component.AudioClipPath.find_last_of("/\\") + 1);
                ImGui::Button(clipName.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                    {
                        std::string path = (const char*)p->Data;
                        if (path.ends_with(".wav") || path.ends_with(".mp3") || path.ends_with(".flac"))
                        {
                            component.AudioClipPath = path;
                            m = true;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (!component.AudioClipPath.empty())
                {
                    if (ImGui::Button("Clear Audio", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                    {
                        component.AudioClipPath = "";
                        m = true;
                    }
                }

                ImGui::Separator();

                m |= UI::DrawCheckbox("Play On Awake", component.PlayOnAwake);
                m |= UI::DrawCheckbox("Looping", component.Looping);
                m |= UI::DrawFloatControl("Volume", component.Volume, 0.05f, 0.0f, 10.0f);
                m |= UI::DrawFloatControl("Min Distance", component.MinDistance, 0.5f, 0.0f, 1000.0f);
                m |= UI::DrawFloatControl("Max Distance", component.MaxDistance, 0.5f, 0.0f, 1000.0f);

                m_AudioTracker.EndEdit(m_Context, entity, component, m);
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
            {
                CommandHistory::AddAndExecute(CreateScope<RemoveComponentCommand<T>>(m_Context, entity.GetUUID(), component));
            }
        }
    }
}