#include <RXNEngine.h>
#include <RXNEngine/Core/EntryPoint.h>
#include <RXNEngine/Core/VFSSystem.h>
#include <RXNEngine/Scene/SceneManager.h>
#include <RXNEngine/Renderer/SceneRenderer.h>
#include <RXNEngine/Renderer/RenderCommand.h>
#include <RXNEngine/Scripting/ScriptEngine.h>
#include <RXNEngine/Scene/Components.h>
#include <RXNEngine/Serialization/SceneSerializer.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <random>
#include <filesystem>
#include <iostream>

namespace RXNRuntime {

    using namespace RXNEngine;

    class RuntimeLayer : public Layer
    {
    public:
        RuntimeLayer(const std::string& entryScenePath)
            : Layer("RuntimeLayer"), m_RequestedEntryScene(entryScenePath)
        {
        }

        virtual void OnAttach() override
        {
            auto vfs = Application::Get().GetSubsystem<VFSSystem>();
            
            if (std::filesystem::exists("assets.rxnpak"))
                vfs->MountPak("assets.rxnpak");

            if (std::filesystem::exists("audio.rxnpak"))
                vfs->MountPak("audio.rxnpak");

            if (std::filesystem::exists("core.rxnpak"))
                vfs->MountPak("core.rxnpak");

            if (std::filesystem::exists("patch_01.rxnpak"))
                vfs->MountPak("patch_01.rxnpak");

            std::string finalScenePath = "";

#if !defined(RXN_DIST)
            if (!m_RequestedEntryScene.empty())
            {
                finalScenePath = m_RequestedEntryScene;
                RXN_CORE_INFO("Boot Config: Using command-line entry scene override: '{0}'", finalScenePath);
            }
            else if (std::filesystem::exists("app.ini"))
            {
                std::ifstream ini("app.ini");
                std::string line;
                while (std::getline(ini, line))
                {
                    if (line.rfind("entry-scene=", 0) == 0)
                    {
                        finalScenePath = line.substr(12);
                        break;
                    }
                }
                RXN_CORE_INFO("Boot Config: Using local app.ini entry scene override: '{0}'", finalScenePath);
            }
            else
#endif
            if (!vfs->GetBakedEntryScene().empty())
            {
                finalScenePath = vfs->GetBakedEntryScene();
                RXN_CORE_INFO("Boot Config: Using baked pak file entry scene: '{0}'", finalScenePath);
            }
            else
            {
                finalScenePath = "res/scenes/start.rxnbin";
                RXN_CORE_INFO("Boot Config: No entry scene configured. Falling back to default: '{0}'", finalScenePath);
            }

            const char* vertexShaderSource = R"(
                #version 330 core
                layout (location = 0) in vec2 a_Position;
                layout (location = 1) in vec2 a_TexCoord;
                out vec2 v_TexCoord;
                void main() {
                    v_TexCoord = a_TexCoord;
                    gl_Position = vec4(a_Position, 0.0, 1.0);
                }
            )";

            const char* fragmentShaderSource = R"(
                #version 330 core
                out vec4 color;
                in vec2 v_TexCoord;
                uniform sampler2D u_Texture;
                void main() {
                    color = texture(u_Texture, v_TexCoord);
                }
            )";

            m_BlitShader = Shader::Create("BlitShader", vertexShaderSource, fragmentShaderSource);

            float quadVertices[] = {
                -1.0f, -1.0f, 0.0f, 0.0f,  
                 1.0f, -1.0f, 1.0f, 0.0f,
                 1.0f,  1.0f, 1.0f, 1.0f, 
                -1.0f,  1.0f, 0.0f, 1.0f
            };
            uint32_t quadIndices[] = { 0, 1, 2, 2, 3, 0 };

            m_BlitVAO = VertexArray::Create();
            Ref<VertexBuffer> vb = VertexBuffer::Create(quadVertices, sizeof(quadVertices));
            vb->SetLayout({ { ShaderDataType::Float2, "a_Position" }, { ShaderDataType::Float2, "a_TexCoord" } });
            m_BlitVAO->AddVertexBuffer(vb);
            m_BlitVAO->SetIndexBuffer(IndexBuffer::Create(quadIndices, 6));

            auto sceneManager = Application::Get().GetSubsystem<SceneManager>();
            sceneManager->LoadScene(finalScenePath);
        }

        virtual void OnDetach() override
        {
            m_BlitShader = nullptr;
        }

        virtual void OnUpdate(float deltaTime) override
        {
            auto sceneManager = Application::Get().GetSubsystem<SceneManager>();
            auto activeScene = sceneManager->GetActiveScene();

            if (!activeScene)
                return;

            activeScene->OnUpdateRuntime(deltaTime);
            activeScene->OnUpdateSimulation(deltaTime);

            if (!m_SceneRenderer || m_SceneRenderer->GetScene() != activeScene)
                m_SceneRenderer = CreateRef<SceneRenderer>(activeScene);

            uint32_t width = Application::Get().GetWindow().GetWidth();
            uint32_t height = Application::Get().GetWindow().GetHeight();

            m_SceneRenderer->RenderRuntime(width, height);

            RenderCommand::BindDefaultRenderTarget();
            RenderCommand::SetViewport(0, 0, width, height);
            RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 1.0f });
            RenderCommand::Clear();
            RenderCommand::SetDepthTest(false);

            m_BlitShader->Bind();
            RenderCommand::BindTextureID(0, m_SceneRenderer->GetFinalColorAttachmentRendererID());
            m_BlitShader->SetInt("u_Texture", 0);

            m_BlitVAO->Bind();
            RenderCommand::DrawIndexed(m_BlitVAO);
        }

    private:
        std::string m_RequestedEntryScene;
        Ref<SceneRenderer> m_SceneRenderer;
        
        Ref<Shader> m_BlitShader;
        Ref<VertexArray> m_BlitVAO;
    };

    class RXNRuntime : public Application
    {
    public:
        RXNRuntime(const WindowProps& props, ApplicationCommandLineArgs args, const std::string& entryScenePath)
            : Application(props, args, false)
        {
            PushLayer(new RuntimeLayer(entryScenePath));
        }
    };
}

namespace RXNEngine {

    Application* CreateApplication(ApplicationCommandLineArgs args)
    {
        std::string entryScenePath = "";

#if !defined(RXN_DIST)
        for (int i = 1; i < args.Count; i++)
        {
            if (strcmp(args[i], "--entry-scene") == 0 && i + 1 < args.Count)
            {
                entryScenePath = args[i + 1];
                break;
            }
        }
#endif

        WindowProps props;
        props.Title = "RXN Standalone Player";
        props.Width = 1280;
        props.Height = 720;
        props.Mode = WindowMode::Windowed;

        return new RXNRuntime::RXNRuntime(props, args, entryScenePath);
    }
}
