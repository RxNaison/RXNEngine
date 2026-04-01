#include "rxnpch.h"
#include "SceneHierarchyPanel.h"
#include "UI.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

#include "RXNEngine/Asset/ModelImporter.h"
#include "RXNEngine/Scripting/ScriptEngine.h"

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
        m_SelectedEntity = {};
    }

    void SceneHierarchyPanel::OnImGuiRender()
    {
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

        ImGui::Begin("Properties");
        if (m_SelectedEntity)
        {
            if (m_SelectedEntity.IsValid())
                DrawComponents(m_SelectedEntity);
            else
                m_SelectedEntity = {};
        }
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

        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            if (!ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left))
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
        uint64_t id = entity.GetComponent<IDComponent>().ID;

        ImGui::TextDisabled(std::to_string(id).c_str());

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

        ImGui::SameLine();
        ImGui::PushItemWidth(-1);
        if (ImGui::Button("Add Component"))
            ImGui::OpenPopup("AddComponent");

        if (ImGui::BeginPopup("AddComponent"))
        {
            ShowAddComponentEntry<CameraComponent>("Camera", m_SelectedEntity);
            ShowAddComponentEntry<StaticMeshComponent>("Mesh", m_SelectedEntity);
            ShowAddComponentEntry<DirectionalLightComponent>("Directional Light", m_SelectedEntity);
            ShowAddComponentEntry<PointLightComponent>("Point Light", m_SelectedEntity);
            ShowAddComponentEntry<RigidbodyComponent>("Rigidbody", m_SelectedEntity);
            ShowAddComponentEntry<BoxColliderComponent>("Box Collider", m_SelectedEntity);
            ShowAddComponentEntry<SphereColliderComponent>("Sphere Collider", m_SelectedEntity);
            ShowAddComponentEntry<CapsuleColliderComponent>("Capsule Collider", m_SelectedEntity);
            ShowAddComponentEntry<MeshColliderComponent>("Mesh Collider", m_SelectedEntity);
            ShowAddComponentEntry<ScriptComponent>("Script Component", m_SelectedEntity, m_SelectedEntity.GetComponent<TagComponent>().Tag);

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


        DrawComponent<StaticMeshComponent>("Mesh", entity, [](auto& component)
            {
                ImGui::Columns(2);
                ImGui::SetColumnWidth(0, 100.0f);
                ImGui::Text("Mesh Asset");
                ImGui::NextColumn();

                std::string label = component.Mesh ? component.AssetPath.substr(component.AssetPath.find_last_of("/\\") + 1) : "Drop Model Here";

				auto assetManager = Application::Get().GetSubsystem<AssetManager>();

                if (ImGui::Button(label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                {
                    std::string path = FileDialogs::OpenFile("*.obj *fbx *.glb *.gltf");
                    if (!path.empty())
                    {
                        component.AssetPath = path;
                        component.Mesh = assetManager->GetMesh(path);
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
                            component.Mesh = assetManager->GetMesh(path);
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
                    if (UI::DrawCheckbox("Override Default Material", hasOverride, 175.0f))
                    {
                        if (hasOverride)
                        {
                            uint32_t matIndex = component.Mesh->GetSubmeshes()[component.SubmeshIndex].MaterialIndex;
                            Ref<Material> defaultMat = component.Mesh->GetMaterials()[matIndex];

                            component.MaterialTableOverride = Material::CreateDefault(defaultMat->GetShader());

                            component.MaterialTableOverride->SetTransparent(defaultMat->IsTransparent());

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

                    if (!hasOverride)
                        ImGui::BeginDisabled();

                    auto params = activeMaterial->GetParameters();

                    if (UI::DrawColor4Control("Albedo", params.AlbedoColor))
                    {
                        activeMaterial->SetAlbedoColor(params.AlbedoColor);
                        activeMaterial->SetTransparent(params.AlbedoColor.a < 1.0f);
                    }

                    if (UI::DrawFloatControl("Roughness", params.Roughness, 0.0f, 1.0f))
                        activeMaterial->SetRoughness(params.Roughness);

                    if (UI::DrawFloatControl("Metalness", params.Metalness, 0.0f, 1.0f))
                        activeMaterial->SetMetalness(params.Metalness);

                    if (UI::DrawFloatControl("AO Strength", params.AO, 0.01f, 0.0f, 1.0f))
                        activeMaterial->SetAO(params.AO);

                    if (UI::DrawColor3Control("Emissive", params.EmissiveColor))
                        activeMaterial->SetEmissiveColor(params.EmissiveColor);

                    if (!hasOverride) ImGui::EndDisabled();
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
                UI::DrawFloatControl("Falloff", component.Falloff, 0.01f, 0.0f, 1.0f);
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

        DrawComponent<BoxColliderComponent>("Box Collider", entity, [](auto& component)
            {
                UI::DrawVec3Control("HalfExtents", component.HalfExtents, 0.5f);
                UI::DrawVec3Control("Offset", component.Offset);
                UI::DrawFloatControl("Static Friction", component.StaticFriction, 0.01f, 0.0f, 1.0f);
                UI::DrawFloatControl("Dynamic Friction", component.DynamicFriction, 0.01f, 0.0f, 1.0f, 117.0f);
                UI::DrawFloatControl("Restitution", component.Restitution, 0.01f, 0.0f, 1.0f);
                UI::DrawCheckbox("Is Trigger", component.IsTrigger);
            });
        DrawComponent<SphereColliderComponent>("Sphere Collider", entity, [](auto& component)
            {
                UI::DrawFloatControl("Radius", component.Radius, 0.05f, 0.0f, 100.0f);
                UI::DrawVec3Control("Offset", component.Offset);
                UI::DrawFloatControl("Static Friction", component.StaticFriction, 0.01f, 0.0f, 1.0f);
                UI::DrawFloatControl("Dynamic Friction", component.DynamicFriction, 0.01f, 0.0f, 1.0f, 117.0f);
                UI::DrawFloatControl("Restitution", component.Restitution, 0.01f, 0.0f, 1.0f);
                UI::DrawCheckbox("Is Trigger", component.IsTrigger);
            });

        DrawComponent<CapsuleColliderComponent>("Capsule Collider", entity, [](auto& component)
            {
                UI::DrawFloatControl("Radius", component.Radius, 0.05f, 0.0f, 100.0f);
                UI::DrawFloatControl("Height", component.Height, 0.05f, 0.0f, 100.0f);
                UI::DrawVec3Control("Offset", component.Offset);
                UI::DrawFloatControl("Static Friction", component.StaticFriction, 0.01f, 0.0f, 1.0f);
                UI::DrawFloatControl("Dynamic Friction", component.DynamicFriction, 0.01f, 0.0f, 1.0f, 117.0f);
                UI::DrawFloatControl("Restitution", component.Restitution, 0.01f, 0.0f, 1.0f);
                UI::DrawCheckbox("Is Trigger", component.IsTrigger);
            });

        DrawComponent<MeshColliderComponent>("Mesh Collider", entity, [](auto& component)
            {
                UI::DrawCheckbox("Is Convex", component.IsConvex);
                UI::DrawFloatControl("Static Friction", component.StaticFriction, 0.01f, 0.0f, 1.0f);
                UI::DrawFloatControl("Dynamic Friction", component.DynamicFriction, 0.01f, 0.0f, 1.0f, 117.0f);
                UI::DrawFloatControl("Restitution", component.Restitution, 0.01f, 0.0f, 1.0f);
                UI::DrawCheckbox("Is Trigger", component.IsTrigger);
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
                {
                    ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "Warning: Class does not exist in loaded Assembly!");
                }

                if (scriptSys->EntityClassExists(component.ClassName))
                {
                    if (m_Context->IsRunning())
                    {
                        auto instance = scriptSys->GetEntityScriptInstance(entity.GetUUID());
                        if (instance)
                        {
                            const auto& fields = scriptSys->GetClassFields(component.ClassName);
                            for (const auto& field : fields)
                            {
                                switch (field.Type)
                                {
                                    case ScriptFieldType::Float:
                                    {
                                        float data = instance->GetFieldValue<float>(field.Name);
                                        if (UI::DrawFloatControl(field.Name.c_str(), data, 0.1f))
                                            instance->SetFieldValue(field.Name, data);
                                        break;
                                    }
                                    case ScriptFieldType::Bool:
                                    {
                                        bool data = instance->GetFieldValue<bool>(field.Name);
                                        if (UI::DrawCheckbox(field.Name.c_str(), data))
                                            instance->SetFieldValue(field.Name, data);
                                        break;
                                    }
                                    case ScriptFieldType::Int:
                                    {
                                        int data = instance->GetFieldValue<int>(field.Name);
                                        if (UI::DrawIntControl(field.Name.c_str(), data))
                                            instance->SetFieldValue(field.Name, data);
                                        break;
                                    }
                                    case ScriptFieldType::Double:
                                    {
                                        double data = instance->GetFieldValue<double>(field.Name);
                                        if (ImGui::DragScalar(field.Name.c_str(), ImGuiDataType_Double, &data, 0.1f))
                                            instance->SetFieldValue(field.Name, data);
                                        break;
                                    }
                                    case ScriptFieldType::Vector2:
                                    {
                                        glm::vec2 data = instance->GetFieldValue<glm::vec2>(field.Name);
                                        if (UI::DrawVec2Control(field.Name.c_str(), data, 0.1f))
                                            instance->SetFieldValue(field.Name, data);
                                        break;
                                    }
                                    case ScriptFieldType::Vector3:
                                    {
                                        glm::vec3 data = instance->GetFieldValue<glm::vec3>(field.Name);
                                        if (UI::DrawVec3Control(field.Name.c_str(), data, 0.1f))
                                            instance->SetFieldValue(field.Name, data);
                                        break;
                                    }
                                    case ScriptFieldType::Vector4:
                                    {
                                        glm::vec4 data = instance->GetFieldValue<glm::vec4>(field.Name);

                                        if (field.Name.find("Color") != std::string::npos)
                                        {
                                            if (UI::DrawColor4Control(field.Name.c_str(), data))
                                                instance->SetFieldValue(field.Name, data);
                                        }
                                        else
                                        {
                                            if (UI::DrawColor4Control(field.Name.c_str(), data, 0.1f))
                                                instance->SetFieldValue(field.Name, data);
                                        }
                                        break;
                                    }
                                    case ScriptFieldType::Entity:
                                    {
                                        ImGui::Text("%s", field.Name.c_str());
                                        ImGui::SameLine();

                                        uint64_t targetID = instance->GetFieldValue<uint64_t>(field.Name);
                                        std::string buttonText = targetID == 0 ? "None (Entity)" : std::to_string(targetID);

                                        if (targetID != 0)
                                        {
                                            Entity targetEntity = m_Context->GetEntityByUUID(targetID);
                                            if (targetEntity)
                                                buttonText = targetEntity.GetComponent<TagComponent>().Tag;
                                            else
                                                buttonText = "Invalid Entity";
                                        }

                                        ImGui::Button(buttonText.c_str(), ImVec2(100.0f, 0.0f));

                                        if (ImGui::BeginDragDropTarget())
                                        {
                                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY"))
                                            {
                                                UUID droppedEntityID = *(UUID*)payload->Data;
                                                instance->SetFieldValue(field.Name, droppedEntityID);
                                            }
                                            ImGui::EndDragDropTarget();
                                        }
                                        break;
                                    }
                                    default:
                                        break;
                                }
                            }
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("Variables can be edited while playing.");
                    }
                }
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