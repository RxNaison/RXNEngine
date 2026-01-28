#include "rxnpch.h"
#include "EditorLayer.h"

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
        RenderTargetSpecification fbSpec;
		fbSpec.Attachments = { RenderTargetTextureFormat::RGBA16F, RenderTargetTextureFormat::Depth };
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		m_RenderTarget = RenderTarget::Create(fbSpec);

        m_ModelShader = Shader::Create("assets/shaders/pbr.glsl");
		m_Camera = CreateRef<EditorCamera>(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);

        m_ActiveScene = CreateRef<Scene>();
        m_CameraEntity = m_ActiveScene->CreateEntity("Scene Camera");
        m_CameraEntity.AddComponent<CameraComponent>();
        m_CameraEntity.GetComponent<CameraComponent>().Camera.SetPerspective(glm::radians(45.0f), 0.01f, 500.0f);

        class CameraControllerScript : public ScriptableEntity
        {
        public:
            void OnUpdate(float ts)
            {
                auto& translation = GetComponent<TransformComponent>().Translation;
                float speed = 5.0f * ts;

                if (Input::IsKeyPressed(KeyCode::W))
                    translation.z -= speed;
                if (Input::IsKeyPressed(KeyCode::A))
                    translation.x -= speed;
                if (Input::IsKeyPressed(KeyCode::S))
                    translation.z += speed;
                if (Input::IsKeyPressed(KeyCode::D))
                    translation.x += speed;
                if (Input::IsKeyPressed(KeyCode::Space))
                    translation.y += speed;
                if (Input::IsKeyPressed(KeyCode::LeftShift))
                    translation.y -= speed;
            }
        };

        m_CameraEntity.AddComponent<NativeScriptComponent>().Bind<CameraControllerScript>();

		m_ModelEntity = m_ActiveScene->CreateEntity("Model Entity");
		m_ModelEntity.AddComponent<MeshComponent>().ModelResource = CreateRef<Model>("assets/models/porsche/scene.gltf", m_ModelShader);

        m_SkyboxEntity = m_ActiveScene->CreateEntity("Skybox Entity");
		m_SkyboxEntity.AddComponent<SkyboxComponent>().Texture = Cubemap::Create("assets/textures/skyboxes/autumn_hill_view_4k.hdr");

		m_Panel.SetContext(m_ActiveScene);
	}

	void EditorLayer::OnDetach()
	{
	}

	void EditorLayer::OnUpdate(float deltaTime)
	{

        m_Camera->OnUpdate(deltaTime);

        m_ActiveScene->OnRenderEditor(deltaTime, *m_Camera, m_RenderTarget);
        //m_ActiveScene->OnUpdateRuntime(deltaTime, m_RenderTarget);

        m_FPS = 1.0f / deltaTime;
	}

	void EditorLayer::OnFixedUpdate(float fixedDeltaTime)
	{
	}

	void EditorLayer::OnEvent(Event& event)
	{
        m_Camera->OnEvent(event);

        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e)->bool { return OnKeyPressed(e); });

        dispatcher.Dispatch<MouseButtonPressedEvent>([this](MouseButtonPressedEvent& e) -> bool
            {
                if (e.GetMouseButton() == MouseCode::ButtonLeft)
                {
                    // Only pick if we are hovering the viewport and NOT using a Gizmo
                    if (m_ViewportHovered && !ImGuizmo::IsOver() && !Input::IsKeyPressed(KeyCode::LeftAlt))
                    {
                        auto [mx, my] = ImGui::GetMousePos(); // Global mouse pos

                        // Convert global mouse to viewport-relative mouse
                        mx -= m_ViewportBounds[0].x;
                        my -= m_ViewportBounds[0].y;

                        // Ensure click is actually inside viewport bounds
                        glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
                        if (mx >= 0 && my >= 0 && mx < viewportSize.x && my < viewportSize.y)
                        {
                            Ray ray = CastRayFromMouse(mx, my);
                            Entity pickedEntity = m_ActiveScene->GetEntityByRay(ray);

                            // Update Selection
                            m_Panel.SetSelectedEntity(pickedEntity);
                            return true; // Consumed
                        }
                    }
                }
                return false;
            });
	}

	void EditorLayer::OnImGuiRenderer()
	{
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

        m_Panel.OnImGuiRender();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
        ImGui::Begin("Renderer");

        auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
        auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
        auto viewportOffset = ImGui::GetWindowPos();

        // Calculate strict bounds of the viewport image
        m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
        m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

        m_ViewportFocused = ImGui::IsWindowFocused();
        m_ViewportHovered = ImGui::IsWindowHovered();
        Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused && !m_ViewportHovered);

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        const auto& renderTargetSpec = m_RenderTarget->GetSpecification();

        if (renderTargetSpec.Width != viewportSize.x || renderTargetSpec.Height != viewportSize.y)
        {
            m_RenderTarget->Resize(viewportSize.x, viewportSize.y);
            m_Camera->SetViewportSize(viewportSize.x, viewportSize.y);
            m_ActiveScene->OnViewportResize(viewportSize.x, viewportSize.y);
        }

        uint32_t textureID = m_RenderTarget->GetColorAttachmentRendererID();
        ImGui::Image((void*)textureID, ImVec2{ renderTargetSpec.Width, renderTargetSpec.Height }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

		Entity selectedEntity = m_Panel.GetSelectedEntity();
        if (selectedEntity && m_GizmoType != -1)
        {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
			ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

            const glm::mat4& cameraView = m_Camera->GetViewMatrix();
            const glm::mat4& cameraProj = m_Camera->GetProjection();

            auto& entityTC = selectedEntity.GetComponent<TransformComponent>();

            glm::mat4 entityTransform = entityTC.GetTransform();

            bool snap = Input::IsKeyPressed(KeyCode::LeftControl);
            float snapValue = m_GizmoType == ImGuizmo::OPERATION::ROTATE ? 10.0f : 0.5f;
            float snapValues[3] = { snapValue, snapValue, snapValue };

            ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProj), (ImGuizmo::OPERATION)m_GizmoType,
                ImGuizmo::LOCAL, glm::value_ptr(entityTransform), nullptr, snap ? snapValues : nullptr);

            if (ImGuizmo::IsUsing())
            {
                glm::vec3 translation, rotation, scale;
                Math::DecomposeTransform(entityTransform, translation, rotation, scale);

                entityTC.Translation = translation;
                entityTC.Rotation = rotation;
                entityTC.Scale = scale;
            }

        }

        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::Begin("Stats");
        
        ImGui::Text(std::to_string(m_FPS).c_str());
        ImGui::End();

        ImGui::End();

    }

    bool EditorLayer::OnKeyPressed(KeyPressedEvent& e)
    {
        // Shortcuts
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
                        SaveSceneAs(FileDialogs::SaveFile("Scene (*.rxns)\0*.rxns\0"));
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
                if (!ImGuizmo::IsUsing())
                    m_GizmoType = ImGuizmo::OPERATION::SCALE;
                break;
            }
        }

        return false;
    }

    Ray EditorLayer::CastRayFromMouse(float mx, float my)
    {

        float x = (2.0f * mx) / m_RenderTarget->GetSpecification().Width - 1.0f;
        float y = 1.0f - (2.0f * my) / m_RenderTarget->GetSpecification().Height;

        glm::vec4 ray_clip = glm::vec4(x, y, -1.0f, 1.0f);

        glm::vec4 ray_eye = glm::inverse(m_Camera->GetProjection()) * ray_clip;
        ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

        glm::vec3 ray_wor = glm::vec3(glm::inverse(m_Camera->GetViewMatrix()) * ray_eye);
        ray_wor = glm::normalize(ray_wor);

        return { m_Camera->GetPosition(), ray_wor };
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
        if (!path.empty())
        {
            m_ActiveScene = CreateRef<Scene>();
            SceneSerializer m_SceneSerializer(m_ActiveScene);
            m_SceneSerializer.Deserialize(path);
            m_Panel.SetContext(m_ActiveScene);
        }
    }
    void EditorLayer::NewScene()
    {
        m_ActiveScene = CreateRef<Scene>();
        m_Panel.SetContext(m_ActiveScene);
    }

}