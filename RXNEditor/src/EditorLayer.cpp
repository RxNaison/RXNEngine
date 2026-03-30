#include "rxnpch.h"
#include "EditorLayer.h"
#include "RXNEngine/Renderer/ModelImporter.h"
#include "RXNEngine/Scripting/ScriptEngine.h"

#include <imgui.h>
#include <ImGuizmo.h>

#include <glm/gtc/type_ptr.hpp>

namespace RXNEditor {

	EditorLayer::EditorLayer(const std::string& name)
		: Layer(name)
	{
	}

	EditorLayer::~EditorLayer()
	{
	}

	void EditorLayer::OnAttach()
	{
		m_EditorCamera = CreateRef<EditorCamera>(45.0f, 1280.0f / 720.0f, 0.1f, 10000.0f);

        m_EditorScene = CreateRef<Scene>();
        m_ActiveScene = m_EditorScene;

        m_SceneRenderer = CreateRef<SceneRenderer>(m_ActiveScene);

		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
        m_EnvironmentPanel.SetContext(m_SceneRenderer);

        m_SceneHierarchyPanel.SetMeshDropCallback([this](const std::string& path)
            {
                m_PendingImportPath = path;
                m_ShowImportDialog = true;
            });
	}

	void EditorLayer::OnDetach()
	{
	}

	void EditorLayer::OnUpdate(float deltaTime)
	{
        OPTICK_EVENT();

        Renderer::ResetStats();
        AssetManager::Update();
		Input::Update();

        switch (m_SceneState)
        {
            case SceneState::Edit:
            {
                m_EditorCamera->OnUpdate(deltaTime);
                m_SceneRenderer->RenderEditor(*m_EditorCamera, m_SceneHierarchyPanel.GetSelectedEntity());
                ScriptEngine::ReloadIfModified(deltaTime);
                break;
            }
            case SceneState::Simulate:
            {
                m_EditorCamera->OnUpdate(deltaTime);
                m_SceneRenderer->RenderEditor(*m_EditorCamera, m_SceneHierarchyPanel.GetSelectedEntity());
                break;
            }
            case SceneState::Play:
            {
                m_ActiveScene->OnUpdateRuntime(deltaTime);
                m_SceneRenderer->RenderRuntime();
                break;
            }
        }

        m_FPS = 1.0f / deltaTime;
	}

	void EditorLayer::OnFixedUpdate(float fixedDeltaTime)
	{
        if (m_SceneState == SceneState::Play || m_SceneState == SceneState::Simulate)
        {
            m_ActiveScene->OnUpdateSimulation(fixedDeltaTime);
        }
	}

	void EditorLayer::OnEvent(Event& event)
	{
        OPTICK_EVENT();

        m_EditorCamera->OnEvent(event);

        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e)->bool { return OnKeyPressed(e); });

        dispatcher.Dispatch<MouseButtonPressedEvent>([this](MouseButtonPressedEvent& e) -> bool
            {
                if (e.GetMouseButton() == MouseCode::ButtonLeft)
                {
                    if (m_ViewportHovered && !ImGuizmo::IsOver() && !Input::IsKeyPressed(KeyCode::LeftAlt))
                    {
                        auto [mx, my] = ImGui::GetMousePos();

                        mx -= m_ViewportBounds[0].x;
                        my -= m_ViewportBounds[0].y;

                        glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
                        if (mx >= 0 && my >= 0 && mx < viewportSize.x && my < viewportSize.y)
                        {
                            my = viewportSize.y - my;

                            int pickedID = m_SceneRenderer->GetEntityIDAtMouse((int)mx, (int)my, *m_EditorCamera);

                            if (pickedID > -1)
                            {
                                Entity pickedEntity = { (entt::entity)pickedID, m_ActiveScene.get() };
                                pickedEntity = m_SceneHierarchyPanel.ResolvePickedEntity(pickedEntity);

                                m_SceneHierarchyPanel.SetSelectedEntity(pickedEntity);
                            }
                            else
                            {
                                m_SceneHierarchyPanel.SetSelectedEntity({});
                            }
                        }
                    }
                }
                return false;
            });
	}

	void EditorLayer::OnImGuiRenderer()
	{
        OPTICK_EVENT();

        static bool opt_fullscreen = true;
        static bool opt_padding = false;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }
        else
        {
            dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
        }

        // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
        // and handle the pass-thru hole, so we ask Begin() to not render a background.
        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
        // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
        // all active windows docked into it will lose their parent and become undocked.
        // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
        // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
        bool p_open = true;
        if (!opt_padding)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", &p_open, window_flags);
        if (!opt_padding)
            ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        // Submit the DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Options"))
            {
                if (ImGui::MenuItem("Close", NULL, false, p_open != NULL))
					Application::Get().Close();

                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New", NULL, false, p_open != NULL))
                {
                    NewScene();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Save As...", NULL, false, p_open != NULL))
                {
                    std::string path = FileDialogs::SaveFile("Scene (*.rxns)\0*.rxns\0");

                    SaveSceneAs(path);
                }
                ImGui::Separator();

                if (ImGui::MenuItem("Open", NULL, false, p_open != NULL))
                {
                    std::string path = FileDialogs::OpenFile("Scene (*.rxns)\0*.rxns\0");

                    OpenScene(path);
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        m_SceneHierarchyPanel.OnImGuiRender();
        m_ContentBrowserPanel.OnImGuiRender();
        m_EnvironmentPanel.OnImGuiRender();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
        ImGui::Begin("Renderer");

        auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
        auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
        auto viewportOffset = ImGui::GetWindowPos();

        m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
        m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

        m_ViewportFocused = ImGui::IsWindowFocused();
        m_ViewportHovered = ImGui::IsWindowHovered();

        if(!ImGui::IsAnyItemActive())
            Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused && !m_ViewportHovered);
        else
            Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused || !m_ViewportHovered);

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        const auto& renderTargetSpec = m_SceneRenderer->GetFinalPass()->GetSpecification();

        if (renderTargetSpec.Width != viewportSize.x || renderTargetSpec.Height != viewportSize.y)
        {
            m_SceneRenderer->SetViewportSize(viewportSize.x, viewportSize.y);
            m_EditorCamera->SetViewportSize(viewportSize.x, viewportSize.y);
            m_ActiveScene->OnViewportResize(viewportSize.x, viewportSize.y);

            m_ViewportWidth = viewportSize.x;
            m_ViewportHeight = viewportSize.y;
        }

        uint32_t textureID = m_SceneRenderer->GetFinalColorAttachmentRendererID();
        ImGui::Image((void*)textureID, ImVec2{ renderTargetSpec.Width, renderTargetSpec.Height }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
        if (selectedEntity && m_GizmoType != -1 && m_SceneState != SceneState::Play)
        {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
			ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

            const glm::mat4& cameraView = m_EditorCamera->GetViewMatrix();
            const glm::mat4& cameraProj = m_EditorCamera->GetProjection();

            auto& entityTC = selectedEntity.GetComponent<TransformComponent>();
            auto& entityRC = selectedEntity.GetComponent<RelationshipComponent>();

            glm::mat4 entityWorldTransform = m_ActiveScene->GetWorldTransform(selectedEntity);

            bool snap = Input::IsKeyPressed(KeyCode::LeftControl);
            float snapValue = m_GizmoType == ImGuizmo::OPERATION::ROTATE ? 5.0f : 0.5f;
            float snapValues[3] = { snapValue, snapValue, snapValue };

            ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProj), (ImGuizmo::OPERATION)m_GizmoType,
                ImGuizmo::LOCAL, glm::value_ptr(entityWorldTransform), nullptr, snap ? snapValues : nullptr);

            if (ImGuizmo::IsUsing())
            {
                if (entityRC.ParentHandle != 0)
                {
                    Entity parent = m_ActiveScene->GetEntityByUUID(entityRC.ParentHandle);
                    if (parent)
                    {
                        glm::mat4 parentTransform = m_ActiveScene->GetWorldTransform(parent);
                        entityWorldTransform = glm::inverse(parentTransform) * entityWorldTransform;
                    }
                }

                glm::vec3 translation, rotation, scale;
                Math::DecomposeTransform(entityWorldTransform, translation, rotation, scale);

                entityTC.Translation = translation;
                entityTC.Rotation = rotation;
                entityTC.Scale = scale;

                if (m_SceneState == SceneState::Simulate)
                {
                    m_ActiveScene->SyncTransformToPhysics(selectedEntity);
                }
            }

        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
            {
                std::string path = (const char*)payload->Data;

                if (path.contains(".hdr"))
                {
                    Ref<Cubemap> skybox = Cubemap::Create(path);
                    m_ActiveScene->SetSkybox(skybox);
                }
                else if(path.contains(".rxns"))
                {
                    OpenScene(std::filesystem::path(path).string());
                }
                else if (!path.empty() && (path.ends_with(".gltf") || path.ends_with(".glb") || path.ends_with(".obj") || path.ends_with(".fbx")))
                {
                    m_PendingImportPath = path;
                    m_ShowImportDialog = true;
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::Begin("Renderer Statistics");

        auto stats = Renderer::GetStats();

        ImGui::Text("Draw Calls: %d", stats.DrawCalls);
        ImGui::Text("Instances: %d", stats.Instances);
        ImGui::Text("Total Indices: %d", stats.TotalIndices);

        ImGui::Text("Total Triangles: %d", stats.TotalIndices / 3);

        ImGui::Text(std::to_string(m_FPS).c_str());

        ImGui::End();

        if (m_ShowImportDialog)
        {
            ImGui::OpenPopup("Import Model Settings");
            m_ShowImportDialog = false;
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Import Model Settings", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            std::filesystem::path p(m_PendingImportPath);
            ImGui::Text("Target: %s", p.filename().string().c_str());
            ImGui::Separator();

            ImGui::Checkbox("Force Rebuild Cache (.rxn)", &m_ImportSettings.ForceRebuildCache);
            ImGui::Separator();

            ImGui::Text("Geometry Flags");
            ImGui::Checkbox("Generate Smooth Normals", &m_ImportSettings.GenerateNormals);
            ImGui::Checkbox("Calculate Tangent Space", &m_ImportSettings.CalculateTangents);
            ImGui::Checkbox("Join Identical Vertices", &m_ImportSettings.JoinIdenticalVertices);
            ImGui::Checkbox("Flip UVs (For OpenGL Textures)", &m_ImportSettings.FlipUVs);
            ImGui::Separator();

            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Aggressive Optimizations (May break hierarchy)");
            ImGui::Checkbox("Optimize Graph (Merge Nodes)", &m_ImportSettings.OptimizeGraph);
            ImGui::Checkbox("Optimize Meshes (Merge Geometries)", &m_ImportSettings.OptimizeMeshes);
            ImGui::Separator();

            if (ImGui::Button("Import", ImVec2(120, 0)))
            {
                Entity importedEntity = ModelImporter::InstantiateToScene(m_ActiveScene, m_PendingImportPath, m_ImportSettings);

                m_SceneHierarchyPanel.SetSelectedEntity(importedEntity);

                ImGui::CloseCurrentPopup();
                m_PendingImportPath.clear();
            }

            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
                m_PendingImportPath.clear();
            }

            ImGui::EndPopup();
        }

        ImGui::End();

    }

    bool EditorLayer::OnKeyPressed(KeyPressedEvent& e)
    {
        if (e.GetRepeatCount() > 1)
            return false;

        bool control = Input::IsKeyPressed(KeyCode::LeftControl) || Input::IsKeyPressed(KeyCode::RightControl);
        bool shift = Input::IsKeyPressed(KeyCode::LeftShift) || Input::IsKeyPressed(KeyCode::RightShift);

        switch (e.GetKeyCode())
        {
            case KeyCode::N:
            {
                if (control)
                    NewScene();

                break;
            }
            case KeyCode::O:
            {
                if (control)
                    OpenScene(FileDialogs::SaveFile("Scene (*.rxns)\0*.rxns\0"));

                break;
            }
            case KeyCode::S:
            {
                if (control)
                {
                    if (shift)
                    {
                        SaveSceneAs(FileDialogs::SaveFile("Scene (*.rxns)\0*.rxns\0"));
                    }
                    else
                    {
                        if(m_ActiveScenePath.empty())
                            SaveSceneAs(FileDialogs::SaveFile("Scene (*.rxns)\0*.rxns\0"));
                        else
                            SaveSceneAs(m_ActiveScenePath);
                    }
                }

                break;
            }

            case KeyCode::Q:
            {
                if (!ImGuizmo::IsUsing())
                    m_GizmoType = -1;
                break;
            }
            case KeyCode::W:
            {
                if (!ImGuizmo::IsUsing())
                    m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
                break;
            }
            case KeyCode::E:
            {
                if (!ImGuizmo::IsUsing())
                    m_GizmoType = ImGuizmo::OPERATION::ROTATE;
                break;
            }
            case KeyCode::R:
            {
                if (control && shift)
                {
                    ScriptEngine::ReloadAssembly();
                }
                else
                {
                    if (!ImGuizmo::IsUsing())
                        m_GizmoType = ImGuizmo::OPERATION::SCALE;
                }
                break;
            }
            case KeyCode::Delete:
            {
                if (Application::Get().GetImGuiLayer()->GetActiveWidgetID() == 0)
                {
                    Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
                    if (selectedEntity)
                    {
                        m_SceneHierarchyPanel.SetSelectedEntity({});
                        m_ActiveScene->DestroyEntity(selectedEntity);
                    }
                }
                break;
            }
            case KeyCode::P:
            {
				if (control && shift)
                {
                    if (m_SceneState == SceneState::Simulate)
                    {
                        OnSceneStop();
                        m_SceneState = SceneState::Edit;
                    }
                    else
                    {
                        OnSceneSimulate();
                        m_SceneState = SceneState::Simulate;
                    }
                }
                else if (control)
                {
                    if (m_SceneState == SceneState::Play)
                    {
                        OnSceneStop();
                        m_SceneState = SceneState::Edit;
                    }
                    else
                    {
                        OnScenePlay();
                        m_SceneState = SceneState::Play;
                    }
                }
                break;
            }
            case KeyCode::D:
            {
                if (control)
                {
                    Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
                    if (selectedEntity)
                    {
                        Entity duplicate = m_ActiveScene->DuplicateEntity(selectedEntity);
                        m_SceneHierarchyPanel.SetSelectedEntity(duplicate);
                    }
				}
                break;
            }
        }

        return false;
    }

    Ray EditorLayer::CastRayFromMouse(float mx, float my)
    {
        float x = (2.0f * mx) / m_SceneRenderer->GetFinalPass()->GetSpecification().Width - 1.0f;
        float y = 1.0f - (2.0f * my) / m_SceneRenderer->GetFinalPass()->GetSpecification().Height;

        glm::vec4 ray_clip = glm::vec4(x, y, -1.0f, 1.0f);

        glm::vec4 ray_eye = glm::inverse(m_EditorCamera->GetProjection()) * ray_clip;
        ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

        glm::vec3 ray_wor = glm::vec3(glm::inverse(m_EditorCamera->GetViewMatrix()) * ray_eye);
        ray_wor = glm::normalize(ray_wor);

        return { m_EditorCamera->GetPosition(), ray_wor };
    }

    void EditorLayer::SaveSceneAs(const std::string& path)
    {
        if (!path.empty())
        {
            SceneSerializer m_SceneSerializer(m_ActiveScene);
            m_SceneSerializer.Serialize(path);
        }
    }

    void EditorLayer::OpenScene(const std::string& path)
    {
        if (m_SceneState != SceneState::Edit)
            OnSceneStop();

        if (!path.empty())
        {
            m_EditorScene = CreateRef<Scene>();
            SceneSerializer m_SceneSerializer(m_EditorScene);
            m_SceneSerializer.Deserialize(path);
            m_ActiveScenePath = path;

            m_ActiveScene = m_EditorScene;
            m_SceneHierarchyPanel.SetContext(m_ActiveScene);
            m_SceneRenderer->SetScene(m_ActiveScene);
            m_ActiveScene->OnViewportResize(m_ViewportWidth, m_ViewportHeight);
        }
    }
    void EditorLayer::NewScene()
    {
        if (m_SceneState != SceneState::Edit)
            OnSceneStop();

        m_EditorScene = CreateRef<Scene>();
        m_ActiveScene = m_EditorScene;
        m_SceneHierarchyPanel.SetContext(m_ActiveScene);
        m_SceneRenderer->SetScene(m_ActiveScene);

        m_ActiveScenePath = {};
    }

    void EditorLayer::OnScenePlay()
    {
        m_SceneState = SceneState::Play;

        Entity selected = m_SceneHierarchyPanel.GetSelectedEntity();
        UUID selectedUUID = selected ? selected.GetUUID() : UUID::Null;

        m_ActiveScene = Scene::Copy(m_EditorScene);
        m_ActiveScene->OnRuntimeStart();

        m_SceneHierarchyPanel.SetContext(m_ActiveScene);
        m_SceneRenderer->SetScene(m_ActiveScene);

        if (selectedUUID != 0)
            m_SceneHierarchyPanel.SetSelectedEntity(m_ActiveScene->GetEntityByUUID(selectedUUID));
    }

    void EditorLayer::OnSceneStop()
    {
        if (m_SceneState == SceneState::Play)
            m_ActiveScene->OnRuntimeStop();
        else if (m_SceneState == SceneState::Simulate)
            m_ActiveScene->OnSimulationStop();

        m_SceneState = SceneState::Edit;

        Entity selected = m_SceneHierarchyPanel.GetSelectedEntity();
        UUID selectedUUID = selected ? selected.GetUUID() : UUID::Null;

        m_ActiveScene = m_EditorScene;

        m_SceneHierarchyPanel.SetContext(m_ActiveScene);
        m_SceneRenderer->SetScene(m_ActiveScene);

        if (selectedUUID != 0)
            m_SceneHierarchyPanel.SetSelectedEntity(m_ActiveScene->GetEntityByUUID(selectedUUID));

		Application::Get().GetWindow().SetCursorMode(CursorMode::Normal);
    }

    void EditorLayer::OnSceneSimulate()
    {
        m_SceneState = SceneState::Simulate;

        Entity selected = m_SceneHierarchyPanel.GetSelectedEntity();
        UUID selectedUUID = selected ? selected.GetUUID() : UUID::Null;

        m_ActiveScene = Scene::Copy(m_EditorScene);

        m_ActiveScene->OnSimulationStart();

        m_SceneHierarchyPanel.SetContext(m_ActiveScene);
        m_SceneRenderer->SetScene(m_ActiveScene);

        if (selectedUUID != 0)
            m_SceneHierarchyPanel.SetSelectedEntity(m_ActiveScene->GetEntityByUUID(selectedUUID));
    }
}