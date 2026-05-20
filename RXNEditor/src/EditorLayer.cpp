#include "rxnpch.h"
#include "EditorLayer.h"
#include "RXNEngine/Asset/ModelImporter.h"
#include "RXNEngine/Scene/SceneManager.h"
#include "CommandHistory.h"
#include "RXNEngine/Scripting/ScriptEngine.h"
#include "RXNEngine/Audio/AudioSystem.h"
#include "RXNEngine/Project/Project.h"
#include "RXNEngine/Project/ProjectSerializer.h"
#include "RXNEngine/Serialization/PrefabSerializer.h"
#include "RXNEngine/Physics/PhysicsWorld.h"

#include <imgui.h>
#include <ImGuizmo.h>

#include <glm/gtc/type_ptr.hpp>

namespace {
    // Möller-Trumbore ray-triangle intersection algorithm
    bool RayTriangleIntersect(const glm::vec3& orig, const glm::vec3& dir,
        const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float& t)
    {
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 h = glm::cross(dir, edge2);
        float a = glm::dot(edge1, h);
        if (a > -0.0001f && a < 0.0001f)
            return false;

        float f = 1.0f / a;
        glm::vec3 s = orig - v0;
        float u = f * glm::dot(s, h);
        if (u < 0.0f || u > 1.0f)
            return false;

        glm::vec3 q = glm::cross(s, edge1);
        float v = f * glm::dot(dir, q);
        if (v < 0.0f || u + v > 1.0f)
            return false;

        t = f * glm::dot(edge2, q);
        return t > 0.0001f;
    }

    // Fast broadphase check for vertical rays
    bool IntersectVerticalRayAABB(const glm::vec3& orig, const RXNEngine::AABB& aabb)
    {
        if (orig.x < aabb.Min.x || orig.x > aabb.Max.x)
            return false;
        if (orig.z < aabb.Min.z || orig.z > aabb.Max.z)
            return false;
        if (orig.y < aabb.Min.y)
            return false;

        return true;
    }
}

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

        m_PIPRenderer = CreateRef<SceneRenderer>(m_ActiveScene);

        auto loadIcon = [](const std::string& path) { return std::filesystem::exists(path) ? Texture2D::Create(path) : Texture2D::WhiteTexture(); };
        m_IconCamera = loadIcon("res/icons/Camera.png");
        m_IconDirLight = loadIcon("res/icons/DirLight.png");
        m_IconPointLight = loadIcon("res/icons/PointLight.png");
        m_IconSpotLight = loadIcon("res/icons/SpotLight.png");
        m_IconAudio = loadIcon("res/icons/Audio.png");

        m_SceneHierarchyPanel.SetMeshDropCallback([this](const std::string& path)
            {
                m_PendingImportPath = path;
                m_ShowImportDialog = true;
            });
        m_ContentBrowserPanel.SetMaterialOpenCallback([this](const std::string& filepath)
            {
                auto mat = Application::Get().GetSubsystem<AssetManager>()->GetMaterial(filepath);
                m_MaterialEditorPanel.SetContext(mat, filepath);
            });
        m_ContentBrowserPanel.SetPhysicsMaterialOpenCallback([this](const std::string& filepath)
            {
                auto physMat = Application::Get().GetSubsystem<AssetManager>()->GetPhysicsMaterial(filepath);
                m_PhysicsMaterialEditorPanel.SetContext(physMat, filepath);
            });
        m_LauncherPanel.SetProjectLoadedCallback([this](Ref<Project> project)
            {
                Application::Get().GetSubsystem<AudioSystem>()->ApplyProjectSettings(project->GetConfig().UseFMOD);
                m_ContentBrowserPanel.Init();

                if (!project->GetConfig().StartScene.empty())
                    OpenScene((Project::GetProjectDirectory() / project->GetConfig().StartScene).string());
                else
                    NewScene();
            });

        Application::Get().GetImGuiLayer()->BlockEvents(false);

        auto cmdArgs = Application::Get().GetCommandLineArgs();
        if (cmdArgs.Count > 1)
        {
            std::filesystem::path projectPath = cmdArgs[1];

            if (projectPath.extension() == ".rxnproj" && std::filesystem::exists(projectPath))
            {
                Ref<Project> loadedProject = Project::New();
                ProjectSerializer serializer(loadedProject);

                if (serializer.Deserialize(projectPath))
                {
                    RXN_CORE_INFO("Command-Line Launch: Loading project {0}", projectPath.string());

                    m_LauncherPanel.AddRecentProject(projectPath.string());
                    Application::Get().GetSubsystem<AudioSystem>()->ApplyProjectSettings(loadedProject->GetConfig().UseFMOD);
                    m_ContentBrowserPanel.Init();

                    if (!loadedProject->GetConfig().StartScene.empty())
                        OpenScene((Project::GetProjectDirectory() / loadedProject->GetConfig().StartScene).string());
                    else
                        NewScene();
                }
            }
        }
	}

	void EditorLayer::OnDetach()
	{
        CommandHistory::Clear();
	}

	void EditorLayer::OnUpdate(float deltaTime)
	{
        RXN_PROFILE_SCOPE();

        Application::Get().GetSubsystem<Renderer>()->ResetStats();

        if (!Project::GetActive())
        {
            RenderCommand::BindDefaultRenderTarget();
            RenderCommand::SetClearColor({ 0.1f, 0.105f, 0.11f, 1.0f });
            RenderCommand::Clear();
            return;
        }

        auto inputSys = Application::Get().GetSubsystem<Input>();
        bool isAnyMouseButtonDown = inputSys->IsMouseButtonPressed(MouseCode::ButtonRight) ||
            inputSys->IsMouseButtonPressed(MouseCode::ButtonMiddle) ||
            inputSys->IsMouseButtonPressed(MouseCode::ButtonLeft);

        if (!isAnyMouseButtonDown)
            m_AllowViewportCamera = m_ViewportHovered && !ImGuizmo::IsOver();

        if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Simulate)
        {
            if (m_AllowViewportCamera && isAnyMouseButtonDown && !ImGuizmo::IsUsing())
                Application::Get().GetWindow().SetCursorMode(CursorMode::Locked);
            else
                Application::Get().GetWindow().SetCursorMode(CursorMode::Normal);
        }

        if (m_SceneState == SceneState::Play)
        {
            Ref<Scene> smScene = Application::Get().GetSubsystem<SceneManager>()->GetActiveScene();
            if (smScene && smScene != m_ActiveScene)
            {
                m_ActiveScene = smScene;
                m_SceneHierarchyPanel.SetContext(m_ActiveScene);
                m_SceneRenderer->SetScene(m_ActiveScene);
            }
        }

        switch (m_SceneState)
        {
            case SceneState::Edit:
            {
                if (m_AllowViewportCamera)
                    m_EditorCamera->OnUpdate(deltaTime);

                m_ActiveScene->OnUpdateEditor(deltaTime);

                m_SceneRenderer->RenderEditor(m_ViewportWidth, m_ViewportHeight, deltaTime, *m_EditorCamera, m_SceneHierarchyPanel.GetSelectedEntities());

                auto selected = m_SceneHierarchyPanel.GetSelectedEntities();
                if (selected.size() == 1 && selected[0] && selected[0].HasComponent<CameraComponent>())
                {
                    SceneCamera pipCam = selected[0].GetComponent<CameraComponent>().Camera;
                    pipCam.SetViewportSize(320, 180);
                    m_PIPRenderer->SetScene(m_ActiveScene);
                    m_PIPRenderer->RenderToTarget(320, 180, pipCam, m_ActiveScene->GetWorldTransform(selected[0]));
                }

                Application::Get().GetSubsystem<ScriptEngine>()->ReloadIfModified(deltaTime);
                break;
            }
            case SceneState::Simulate:
            {
                if (m_AllowViewportCamera)
                    m_EditorCamera->OnUpdate(deltaTime);

                m_ActiveScene->OnUpdateEditor(deltaTime);

                m_SceneRenderer->RenderEditor(m_ViewportWidth, m_ViewportHeight, deltaTime, *m_EditorCamera, m_SceneHierarchyPanel.GetSelectedEntities());
                break;
            }
            case SceneState::Play:
            {
                m_ActiveScene->OnUpdateRuntime(deltaTime);
                m_SceneRenderer->RenderRuntime(m_ViewportWidth, m_ViewportHeight);
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
        RXN_PROFILE_SCOPE();

        if (!Project::GetActive())
            return;

        if (m_ViewportHovered)
            m_EditorCamera->OnEvent(event);

        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e)->bool { return OnKeyPressed(e); });

        dispatcher.Dispatch<MouseButtonPressedEvent>([this](MouseButtonPressedEvent& e) -> bool
            {
                if (e.GetMouseButton() == MouseCode::ButtonLeft)
                {
                    if (m_ViewportHovered && !ImGuizmo::IsOver() && !Application::Get().GetSubsystem<Input>()->IsKeyPressed(KeyCode::LeftAlt))
                    {
                        auto [mx, my] = ImGui::GetMousePos();
                        bool isCtrlHeld = Application::Get().GetSubsystem<Input>()->IsKeyPressed(KeyCode::LeftControl);

                        bool hitIcon = false;
                        for (auto it = m_IconHitboxes.rbegin(); it != m_IconHitboxes.rend(); ++it)
                        {
                            if (mx >= it->Min.x && mx <= it->Max.x && my >= it->Min.y && my <= it->Max.y)
                            {
                                hitIcon = true;
                                if (isCtrlHeld)
                                    m_SceneHierarchyPanel.ToggleSelection(it->Ent);
                                else 
                                    m_SceneHierarchyPanel.SetSelectedEntity(it->Ent);
                                break;
                            }
                        }

                        if (!hitIcon)
                        {
                            float relX = mx - m_ViewportBounds[0].x;
                            float relY = my - m_ViewportBounds[0].y;

                            glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
                            if (relX >= 0 && relY >= 0 && relX < viewportSize.x && relY < viewportSize.y)
                            {
                                relY = viewportSize.y - relY;

                                int pickedID = m_SceneRenderer->GetEntityIDAtMouse((int)relX, (int)relY, *m_EditorCamera);

                                if (pickedID > -1)
                                {
                                    Entity pickedEntity = { (entt::entity)pickedID, m_ActiveScene.get() };
                                    pickedEntity = m_SceneHierarchyPanel.ResolvePickedEntity(pickedEntity);

                                    if (isCtrlHeld)
                                        m_SceneHierarchyPanel.ToggleSelection(pickedEntity);
                                    else
                                        m_SceneHierarchyPanel.SetSelectedEntity(pickedEntity);
                                }
                                else
                                {
                                    if (!isCtrlHeld)
                                        m_SceneHierarchyPanel.ClearSelection();
                                }
                            }
                        }
                    }
                }
                return false;
            });
	}

	void EditorLayer::OnImGuiRenderer()
	{
        RXN_PROFILE_SCOPE();

        if (!Project::GetActive())
        {
            m_LauncherPanel.OnImGuiRender();
            return;
        }

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
        m_MaterialEditorPanel.OnImGuiRender();
        m_PhysicsMaterialEditorPanel.OnImGuiRender();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
        ImGui::Begin("Renderer");

        auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
        auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
        auto viewportOffset = ImGui::GetWindowPos();

        m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
        m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

        m_ViewportFocused = ImGui::IsWindowFocused();
        m_ViewportHovered = ImGui::IsWindowHovered();

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        const auto& renderTargetSpec = m_SceneRenderer->GetFinalPass()->GetSpecification();

        m_ViewportWidth = (uint32_t)viewportSize.x;
        m_ViewportHeight = (uint32_t)viewportSize.y;

        uint32_t textureID = m_SceneRenderer->GetFinalColorAttachmentRendererID();
        ImGui::Image((void*)(uint64_t)textureID, viewportSize, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

        const std::vector<Entity>& selectedEntities = m_SceneHierarchyPanel.GetSelectedEntities();

        if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Simulate)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            m_IconHitboxes.clear();

            auto drawIcon = [&](Entity entity, Ref<Texture2D> icon)
                {
                    glm::mat4 transform = m_ActiveScene->GetWorldTransform(entity);
                    glm::vec3 worldPos = glm::vec3(transform[3]);
                    glm::vec4 clipPos = m_EditorCamera->GetViewProjection() * glm::vec4(worldPos, 1.0f);

                    if (clipPos.w > 0.1f)
                    {
                        glm::vec3 ndcPos = glm::vec3(clipPos) / clipPos.w;
                        if (ndcPos.z > -1.0f && ndcPos.z < 1.0f && ndcPos.x > -1.0f && ndcPos.x < 1.0f && ndcPos.y > -1.0f && ndcPos.y < 1.0f)
                        {
                            float screenX = m_ViewportBounds[0].x + (ndcPos.x + 1.0f) * 0.5f * (m_ViewportBounds[1].x - m_ViewportBounds[0].x);
                            float screenY = m_ViewportBounds[0].y + (1.0f - ndcPos.y) * 0.5f * (m_ViewportBounds[1].y - m_ViewportBounds[0].y);

                            float distance = clipPos.w;
                            float iconSize = glm::clamp(400.0f / distance, 24.0f, 150.0f);

                            ImVec2 minPos = ImVec2(screenX - iconSize / 2, screenY - iconSize / 2);
                            ImVec2 maxPos = ImVec2(screenX + iconSize / 2, screenY + iconSize / 2);

                            drawList->AddImage((void*)(uint64_t)icon->GetRendererID(), minPos, maxPos, ImVec2(0, 1), ImVec2(1, 0));

                            m_IconHitboxes.push_back({ entity, minPos, maxPos });
                        }
                    }
                };

            for (auto e : m_ActiveScene->GetRaw().view<CameraComponent>())
                drawIcon(Entity(e, m_ActiveScene.get()), m_IconCamera);

            for (auto e : m_ActiveScene->GetRaw().view<DirectionalLightComponent>())
                drawIcon(Entity(e, m_ActiveScene.get()), m_IconDirLight);

            for (auto e : m_ActiveScene->GetRaw().view<PointLightComponent>())
                drawIcon(Entity(e, m_ActiveScene.get()), m_IconPointLight);

            for (auto e : m_ActiveScene->GetRaw().view<SpotLightComponent>())
                drawIcon(Entity(e, m_ActiveScene.get()), m_IconSpotLight);

            for (auto e : m_ActiveScene->GetRaw().view<AudioSourceComponent>())
                drawIcon(Entity(e, m_ActiveScene.get()), m_IconAudio);

            if (selectedEntities.size() == 1 && selectedEntities[0].HasComponent<CameraComponent>())
            {
                float pipWidth = 320.0f;
                float pipHeight = 180.0f;
                ImVec2 pipPos = ImVec2(m_ViewportBounds[1].x - pipWidth - 20.0f, m_ViewportBounds[1].y - pipHeight - 20.0f);

                drawList->AddRectFilled(ImVec2(pipPos.x - 2, pipPos.y - 2), ImVec2(pipPos.x + pipWidth + 2, pipPos.y + pipHeight + 2), IM_COL32(50, 50, 50, 255));
                drawList->AddImage((void*)(uint64_t)m_PIPRenderer->GetFinalColorAttachmentRendererID(), pipPos, ImVec2(pipPos.x + pipWidth, pipPos.y + pipHeight), ImVec2(0, 1), ImVec2(1, 0));
                drawList->AddText(ImVec2(pipPos.x + 5, pipPos.y + 5), IM_COL32(255, 255, 255, 255), "Camera Preview");
            }

            for (Entity entity : selectedEntities)
            {
                if (entity.HasComponent<UITransformComponent>())
                {
                    auto& ui = entity.GetComponent<UITransformComponent>();
                    
                    bool isScreenSpace = true;
                    Entity curr = entity;
                    while (curr)
                    {
                        if (curr.HasComponent<UICanvasComponent>())
                        {
                            isScreenSpace = curr.GetComponent<UICanvasComponent>().RenderMode == CanvasRenderMode::ScreenSpaceOverlay;
                            break;
                        }

                        if (curr.HasComponent<RelationshipComponent>())
                        {
                            UUID pid = curr.GetComponent<RelationshipComponent>().ParentHandle;
                            curr = pid != 0 ? m_ActiveScene->GetEntityByUUID(pid) : Entity{};
                        } 
                        else break;
                    }

                    if (isScreenSpace)
                    {
                        float yMin = m_ViewportBounds[1].y - ui.ComputedBoundsMin.y;
                        float yMax = m_ViewportBounds[1].y - ui.ComputedBoundsMax.y;
                        
                        ImVec2 minPos(m_ViewportBounds[0].x + ui.ComputedBoundsMin.x, yMax);
                        ImVec2 maxPos(m_ViewportBounds[0].x + ui.ComputedBoundsMax.x, yMin);
                        
                        drawList->AddRect(minPos, maxPos, IM_COL32(255, 200, 0, 255), 0.0f, 0, 2.0f);
                        
                        glm::vec2 parentSize(0.0f);
                        if (entity.HasComponent<RelationshipComponent>())
                        {
                            UUID pid = entity.GetComponent<RelationshipComponent>().ParentHandle;
                            if (pid != 0)
                            {
                                Entity parent = m_ActiveScene->GetEntityByUUID(pid);
                                if (parent && parent.HasComponent<UITransformComponent>())
                                    parentSize = parent.GetComponent<UITransformComponent>().ComputedSize;
                            }
                        }
                    }
                }
            }
        }

        if (!selectedEntities.empty() && m_GizmoType != -1 && m_SceneState != SceneState::Play)
        {
            Entity primaryEntity = selectedEntities[0];

            bool isUI = false;
            bool isScreenSpace = false;
            
            if (primaryEntity.HasComponent<UITransformComponent>())
            {
                isUI = true;
                
                if (primaryEntity.HasComponent<UICanvasComponent>())
                {
                    if (primaryEntity.GetComponent<UICanvasComponent>().RenderMode == CanvasRenderMode::WorldSpace)
                    {
                        isUI = false;
                    }
                }
                
                if (isUI)
                {
                    Entity curr = primaryEntity;
                    while (curr) {
                        if (curr.HasComponent<UICanvasComponent>()) {
                            isScreenSpace = curr.GetComponent<UICanvasComponent>().RenderMode == CanvasRenderMode::ScreenSpaceOverlay;
                            break;
                        }
                        if (curr.HasComponent<RelationshipComponent>()) {
                            UUID pid = curr.GetComponent<RelationshipComponent>().ParentHandle;
                            curr = pid != 0 ? m_ActiveScene->GetEntityByUUID(pid) : Entity{};
                        } else break;
                    }
                }
            }

            ImGuizmo::SetOrthographic(isScreenSpace);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

            glm::mat4 cameraView = m_EditorCamera->GetViewMatrix();
            glm::mat4 cameraProj = m_EditorCamera->GetProjection();

            if (isScreenSpace)
            {
                cameraProj = glm::ortho(0.0f, (float)m_ViewportWidth, 0.0f, (float)m_ViewportHeight, -1.0f, 1.0f);
                cameraView = glm::mat4(1.0f);
            }

            bool snap = Application::Get().GetSubsystem<Input>()->IsKeyPressed(KeyCode::LeftControl);
            float snapValue = m_GizmoType == ImGuizmo::OPERATION::ROTATE ? 5.0f : 0.5f;
            float snapValues[3] = { snapValue, snapValue, snapValue };

            glm::mat4 groupTransform = isUI ? primaryEntity.GetComponent<UITransformComponent>().ComputedTransform : m_ActiveScene->GetWorldTransform(primaryEntity);
            glm::mat4 deltaMatrix(1.0f);

            bool isUsing = ImGuizmo::IsUsing();
            if (isUsing && !m_WasGizmoUsing)
            {
                m_WasGizmoUsing = true;
                m_GizmoStartTransforms.clear();
                m_GizmoStartUITransforms.clear();
                for (Entity entity : selectedEntities)
                {
                    m_GizmoStartTransforms.push_back({ entity.GetUUID(), entity.GetComponent<TransformComponent>() });
                    if (entity.HasComponent<UITransformComponent>())
                        m_GizmoStartUITransforms.push_back({ entity.GetUUID(), entity.GetComponent<UITransformComponent>() });
                }
            }

            ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProj), (ImGuizmo::OPERATION)m_GizmoType,
                ImGuizmo::LOCAL, glm::value_ptr(groupTransform), glm::value_ptr(deltaMatrix), snap ? snapValues : nullptr);

            if (ImGuizmo::IsUsing())
            {
                glm::mat4 primaryOriginalTransform = isUI ? primaryEntity.GetComponent<UITransformComponent>().ComputedTransform : m_ActiveScene->GetWorldTransform(primaryEntity);
                glm::vec3 pivotTranslation, tempRot, tempScale;
                Math::DecomposeTransform(primaryOriginalTransform, pivotTranslation, tempRot, tempScale);
                glm::mat4 pivot = glm::translate(glm::mat4(1.0f), pivotTranslation);

                for (Entity entity : selectedEntities)
                {
                    if (isUI && entity.HasComponent<UITransformComponent>())
                    {
                        auto& ui = entity.GetComponent<UITransformComponent>();
                        
                        glm::vec3 deltaTranslation, deltaRotation, deltaScale;
                        Math::DecomposeTransform(deltaMatrix, deltaTranslation, deltaRotation, deltaScale);
                        
                        if (m_GizmoType == ImGuizmo::OPERATION::TRANSLATE)
                        {
                            ui.OffsetMin.x += deltaTranslation.x;
                            ui.OffsetMax.x += deltaTranslation.x;
                            ui.OffsetMin.y += deltaTranslation.y;
                            ui.OffsetMax.y += deltaTranslation.y;
                        }
                        else if (m_GizmoType == ImGuizmo::OPERATION::SCALE)
                        {
                            glm::vec2 size = ui.OffsetMax - ui.OffsetMin;
                            glm::vec2 center = ui.OffsetMin + (size * 0.5f);
                            glm::vec2 newSize = size * glm::vec2(deltaScale.x, deltaScale.y);
                            ui.OffsetMin = center - (newSize * 0.5f);
                            ui.OffsetMax = center + (newSize * 0.5f);
                        }
                        ui.IsDirty = true;
                    }
                    else
                    {
                        auto& entityTC = entity.GetComponent<TransformComponent>();
                        auto& entityRC = entity.GetComponent<RelationshipComponent>();

                        glm::mat4 entityWorldTransform;

                        if (entity == primaryEntity)
                        {
                            entityWorldTransform = groupTransform;
                        }
                        else
                        {
                            glm::mat4 oldEntityWorldTransform = m_ActiveScene->GetWorldTransform(entity);
                            entityWorldTransform = pivot * deltaMatrix * glm::inverse(pivot) * oldEntityWorldTransform;
                        }

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
                            RXNEngine::Application::Get().GetSubsystem<RXNEngine::PhysicsWorld>()->SyncTransformToPhysics(entity);
                    }
                }
            }

            if (!isUsing && m_WasGizmoUsing)
            {
                m_WasGizmoUsing = false;

                std::vector<ChangeMultiTransformCommand::TransformData> transforms;
                for (const auto& [id, startTransform] : m_GizmoStartTransforms)
                {
                    Entity entity = m_ActiveScene->GetEntityByUUID(id);
                    if (entity)
                    {
                        TransformComponent endTransform = entity.GetComponent<TransformComponent>();
                        if (startTransform.Translation != endTransform.Translation ||
                            startTransform.Rotation != endTransform.Rotation ||
                            startTransform.Scale != endTransform.Scale)
                        {
                            transforms.push_back({ id, startTransform, endTransform });
                        }
                    }
                }
                if (!transforms.empty())
                    CommandHistory::Push(CreateScope<ChangeMultiTransformCommand>(m_ActiveScene, transforms));
                    
                std::vector<ChangeMultiUITransformCommand::UITransformData> uiTransforms;
                for (const auto& [id, startTransform] : m_GizmoStartUITransforms)
                {
                    Entity entity = m_ActiveScene->GetEntityByUUID(id);
                    if (entity && entity.HasComponent<UITransformComponent>())
                    {
                        UITransformComponent endTransform = entity.GetComponent<UITransformComponent>();
                        if (startTransform.OffsetMin != endTransform.OffsetMin ||
                            startTransform.OffsetMax != endTransform.OffsetMax)
                        {
                            uiTransforms.push_back({ id, startTransform, endTransform });
                        }
                    }
                }
                if (!uiTransforms.empty())
                    CommandHistory::Push(CreateScope<ChangeMultiUITransformCommand>(m_ActiveScene, uiTransforms));
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
                else if (path.ends_with(".rxnpb"))
                {
                    Entity prefabRoot = PrefabSerializer::Deserialize(m_ActiveScene, path);
                    if (prefabRoot)
                    {
                        m_SceneHierarchyPanel.SetSelectedEntity(prefabRoot);
                        CommandHistory::Push(CreateScope<SpawnEntityTreeCommand>(m_ActiveScene, prefabRoot));
                    }
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

        auto stats = Application::Get().GetSubsystem<Renderer>()->GetStats();

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

            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Aggressive Optimizations");
            ImGui::Checkbox("Optimize Graph (Merge Nodes)", &m_ImportSettings.OptimizeGraph);
            ImGui::Checkbox("Optimize Meshes (Merge Geometries)", &m_ImportSettings.OptimizeMeshes);
            ImGui::Checkbox("Generate CoACD Physics Hulls", &m_ImportSettings.GenerateCoACDHulls);

            if (m_ImportSettings.GenerateCoACDHulls)
            {
                ImGui::Indent();
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Warning: Asset baking is a heavy process.");
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "It may freeze the editor for a few minutes.");

                ImGui::SliderFloat("Threshold", &m_ImportSettings.CoACD_Threshold, 0.01f, 1.0f);
                ImGui::SliderInt("Max Hulls", &m_ImportSettings.CoACD_MaxHulls, -1, 100);
                ImGui::SliderInt("Voxel Repair Res", &m_ImportSettings.CoACD_PrepResolution, 20, 100);

                ImGui::Text("MCTS Search (Lower = Faster, Worse Quality)");
                ImGui::SliderInt("Search Nodes", &m_ImportSettings.CoACD_MctsNodes, 10, 40);
                ImGui::SliderInt("Search Iters", &m_ImportSettings.CoACD_MctsIterations, 60, 2000);
                ImGui::SliderInt("Search Depth", &m_ImportSettings.CoACD_MctsMaxDepth, 2, 7);
                ImGui::Unindent();
            }
            ImGui::Separator();

            if (ImGui::Button("Import", ImVec2(120, 0)))
            {
                Entity importedEntity = ModelImporter::InstantiateToScene(m_ActiveScene, m_PendingImportPath, m_ImportSettings);

                m_SceneHierarchyPanel.SetSelectedEntity(importedEntity);

                CommandHistory::Push(CreateScope<SpawnEntityTreeCommand>(m_ActiveScene, importedEntity));

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

        if (ImGui::GetIO().WantTextInput)
            return false;

        int key = e.GetKeyCode();
        auto inputSys = Application::Get().GetSubsystem<Input>();

        bool control = inputSys->IsKeyPressed(KeyCode::LeftControl) || inputSys->IsKeyPressed(KeyCode::RightControl);
        bool shift = inputSys->IsKeyPressed(KeyCode::LeftShift) || inputSys->IsKeyPressed(KeyCode::RightShift);

        bool alt = inputSys->IsKeyPressed(KeyCode::LeftAlt) || inputSys->IsKeyPressed(KeyCode::RightAlt);
        bool rightMouse = inputSys->IsMouseButtonPressed(MouseCode::ButtonRight);

        if (key == KeyCode::P)
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
        }

        if (m_SceneState == SceneState::Play || m_SceneState == SceneState::Simulate)
            return false;

        if (!(alt || rightMouse))
        {
            switch (key)
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
                            if (m_ActiveScenePath.empty())
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
                        Application::Get().GetSubsystem<ScriptEngine>()->ReloadAssembly();
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
                    const auto& selected = m_SceneHierarchyPanel.GetSelectedEntities();
                    if (!selected.empty())
                    {
                        CommandHistory::AddAndExecute(CreateScope<DeleteEntitiesCommand>(m_ActiveScene, selected));
                        m_SceneHierarchyPanel.ClearSelection();
                    }
                    break;
                }
                case KeyCode::D:
                {
                    if (control)
                    {
                        auto selected = m_SceneHierarchyPanel.GetSelectedEntities();
                        m_SceneHierarchyPanel.ClearSelection();

                        std::vector<Entity> topLevelEntities;
                        for (Entity e : selected)
                        {
                            bool hasSelectedAncestor = false;
                            Entity current = e;
                            while (current.GetComponent<RelationshipComponent>().ParentHandle != 0)
                            {
                                Entity parent = m_ActiveScene->GetEntityByUUID(current.GetComponent<RelationshipComponent>().ParentHandle);

                                if (!parent)
                                    break;

                                if (std::find(selected.begin(), selected.end(), parent) != selected.end())
                                {
                                    hasSelectedAncestor = true;
                                    break;
                                }
                                current = parent;
                            }

                            if (!hasSelectedAncestor)
                                topLevelEntities.push_back(e);
                        }

                        for (Entity entity : topLevelEntities)
                        {
                            if (entity)
                            {
                                Entity duplicate = m_ActiveScene->DuplicateEntity(entity);
                                CommandHistory::Push(CreateScope<SpawnEntityTreeCommand>(m_ActiveScene, duplicate));
                                m_SceneHierarchyPanel.ToggleSelection(duplicate);
                            }
                        }
                    }
                    break;
                }
                case KeyCode::Z:
                {
                    if (control)
                    {
                        if (shift)
                            CommandHistory::Redo();
                        else
                            CommandHistory::Undo();
                        return true;
                    }
                    break;
                }
                case KeyCode::Y:
                {
                    if (control)
                    {
                        CommandHistory::Redo();
                        return true;
                    }
                    break;
                }
                case KeyCode::F:
                {
                    const auto& selected = m_SceneHierarchyPanel.GetSelectedEntities();
                    if (!selected.empty())
                    {
                        glm::vec3 center(0.0f);
                        for (Entity entity : selected)
                        {
                            glm::mat4 transform = m_ActiveScene->GetWorldTransform(entity);
                            center += glm::vec3(transform[3]);
                        }

                        center /= (float)selected.size();
                        m_EditorCamera->Focus(center);
                    }
                    break;
                }
                case KeyCode::End:
                {
                    DropSelectedToFloor();
                    break;
                }
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

            m_ActiveScenePath = path;

            if (Project::GetActive())
            {
                std::filesystem::path relativePath = std::filesystem::relative(path, Project::GetProjectDirectory());
                if (Project::GetConfig().StartScene != relativePath)
                {
                    Project::GetConfig().StartScene = relativePath;
                    ProjectSerializer projectSerializer(Project::GetActive());
                    projectSerializer.Serialize(Project::GetProjectFilePath());
                }
            }
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

            if (Project::GetActive())
            {
                std::filesystem::path relativePath = std::filesystem::relative(path, Project::GetProjectDirectory());
                if (Project::GetConfig().StartScene != relativePath)
                {
                    Project::GetConfig().StartScene = relativePath;
                    ProjectSerializer projectSerializer(Project::GetActive());
                    projectSerializer.Serialize(Project::GetProjectFilePath());
                }
            }
        }

        CommandHistory::Clear();
    }

    void EditorLayer::NewScene()
    {
        if (m_SceneState != SceneState::Edit)
            OnSceneStop();

        m_EditorScene = CreateRef<Scene>();
        m_ActiveScene = m_EditorScene;
        m_ActiveScene->OnViewportResize(m_ViewportWidth, m_ViewportHeight);
        m_SceneHierarchyPanel.SetContext(m_ActiveScene);
        m_SceneRenderer->SetScene(m_ActiveScene);

        m_ActiveScenePath = {};
        CommandHistory::Clear();
    }

    void EditorLayer::OnScenePlay()
    {
        m_SceneState = SceneState::Play;

        Entity selected = m_SceneHierarchyPanel.GetSelectedEntity();
        UUID selectedUUID = selected ? selected.GetUUID() : UUID::Null;

        m_ActiveScene = Scene::Copy(m_EditorScene);
        Application::Get().GetSubsystem<SceneManager>()->SetActiveScene(m_ActiveScene);
        m_ActiveScene->OnRuntimeStart();

        m_SceneHierarchyPanel.SetContext(m_ActiveScene);
        m_SceneRenderer->SetScene(m_ActiveScene);

        m_ActiveScene->OnViewportResize(m_ViewportWidth, m_ViewportHeight);

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
        Application::Get().GetSubsystem<SceneManager>()->SetActiveScene(nullptr);

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

        m_ActiveScene->OnViewportResize(m_ViewportWidth, m_ViewportHeight);

        if (selectedUUID != 0)
            m_SceneHierarchyPanel.SetSelectedEntity(m_ActiveScene->GetEntityByUUID(selectedUUID));
    }

    void EditorLayer::DropSelectedToFloor()
    {
        const auto& selected = m_SceneHierarchyPanel.GetSelectedEntities();
        if (selected.empty())
            return;

        std::vector<ChangeMultiTransformCommand::TransformData> transforms;
        auto meshView = m_ActiveScene->GetRaw().view<StaticMeshComponent, TransformComponent>();

        for (Entity entity : selected)
        {
            if (!entity)
                continue;

            glm::mat4 worldTransform = m_ActiveScene->GetWorldTransform(entity);
            glm::vec3 position(worldTransform[3]);

            glm::vec3 minAABB(FLT_MAX);
            glm::vec3 maxAABB(-FLT_MAX);
            bool hasMeshes = false;

            auto findBounds = [&](Entity currentEnt, auto& self) -> void
                {
                    if (currentEnt.HasComponent<StaticMeshComponent>())
                    {
                        auto& mc = currentEnt.GetComponent<StaticMeshComponent>();
                        if (mc.Mesh)
                        {
                            glm::mat4 wt = m_ActiveScene->GetWorldTransform(currentEnt);
                            const auto& submesh = mc.Mesh->GetSubmeshes()[mc.SubmeshIndex];

                            AABB worldAABB = Math::CalculateWorldAABB(submesh.BoundingBox, wt);

                            minAABB = glm::min(minAABB, worldAABB.Min);
                            maxAABB = glm::max(maxAABB, worldAABB.Max);
                            hasMeshes = true;
                        }
                    }

                    if (currentEnt.HasComponent<RelationshipComponent>())
                    {
                        for (UUID childID : currentEnt.GetComponent<RelationshipComponent>().Children)
                        {
                            Entity child = m_ActiveScene->GetEntityByUUID(childID);

                            if (child)
                                self(child, self);
                        }
                    }
                };

            findBounds(entity, findBounds);

            if (!hasMeshes)
                continue;

            glm::vec3 rayOrigin;
            rayOrigin.x = (minAABB.x + maxAABB.x) * 0.5f;
            rayOrigin.z = (minAABB.z + maxAABB.z) * 0.5f;
            rayOrigin.y = maxAABB.y + 1.0f;

            glm::vec3 rayDir(0.0f, -1.0f, 0.0f);
            float closestT = FLT_MAX;
            bool hasHit = false;

            for (auto e : meshView)
            {
                Entity other = { e, m_ActiveScene.get() };

                if (std::find(selected.begin(), selected.end(), other) != selected.end())
                    continue;

                if (m_ActiveScene->IsDescendantOf(other, entity))
                    continue;

                auto& mc = other.GetComponent<StaticMeshComponent>();
                if (!mc.Mesh)
                    continue;

                glm::mat4 otherTransform = m_ActiveScene->GetWorldTransform(other);
                const auto& submesh = mc.Mesh->GetSubmeshes()[mc.SubmeshIndex];
                AABB otherWorldAABB = Math::CalculateWorldAABB(submesh.BoundingBox, otherTransform);

                if (IntersectVerticalRayAABB(rayOrigin, otherWorldAABB))
                {
                    glm::mat4 invTransform = glm::inverse(otherTransform * submesh.LocalTransform);
                    glm::vec3 localRayOrig = glm::vec3(invTransform * glm::vec4(rayOrigin, 1.0f));
                    glm::vec3 localRayDir = glm::normalize(glm::vec3(invTransform * glm::vec4(rayDir, 0.0f)));

                    const auto& vertices = mc.Mesh->GetVertices();
                    const auto& indices = mc.Mesh->GetIndices();

                    for (uint32_t i = 0; i < submesh.IndexCount; i += 3)
                    {
                        uint32_t i0 = indices[submesh.BaseIndex + i];
                        uint32_t i1 = indices[submesh.BaseIndex + i + 1];
                        uint32_t i2 = indices[submesh.BaseIndex + i + 2];

                        float t;
                        if (RayTriangleIntersect(localRayOrig, localRayDir,
                            vertices[i0].Position, vertices[i1].Position, vertices[i2].Position, t))
                        {
                            glm::vec3 localHitPoint = localRayOrig + localRayDir * t;
                            glm::vec3 worldHitPoint = glm::vec3(otherTransform * submesh.LocalTransform * glm::vec4(localHitPoint, 1.0f));

                            float worldT = rayOrigin.y - worldHitPoint.y;

                            if (worldT > -0.001f && worldT < closestT)
                            {
                                closestT = worldT;
                                hasHit = true;
                            }
                        }
                    }
                }
            }

            if (hasHit)
            {
                TransformComponent oldTransform = entity.GetComponent<TransformComponent>();
                TransformComponent newTransform = oldTransform;

                float hitY = rayOrigin.y - closestT;

                float offsetY = hitY - minAABB.y;

                glm::vec3 newWorldPos = position;
                newWorldPos.y += offsetY;

                auto& rc = entity.GetComponent<RelationshipComponent>();
                if (rc.ParentHandle != 0)
                {
                    Entity parent = m_ActiveScene->GetEntityByUUID(rc.ParentHandle);
                    if (parent)
                    {
                        glm::mat4 parentTransform = m_ActiveScene->GetWorldTransform(parent);
                        glm::vec3 localPos = glm::inverse(parentTransform) * glm::vec4(newWorldPos, 1.0f);
                        newTransform.Translation = localPos;
                    }
                }
                else
                {
                    newTransform.Translation.y = newWorldPos.y;
                }

                transforms.push_back({ entity.GetUUID(), oldTransform, newTransform });
            }
        }

        if (!transforms.empty())
            CommandHistory::AddAndExecute(CreateScope<ChangeMultiTransformCommand>(m_ActiveScene, transforms));
    }
}