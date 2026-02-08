#include "rxnpch.h"
#include "SceneRenderer.h"
#include "RXNEngine/Renderer/Renderer.h"
#include "RXNEngine/Renderer/RenderCommand.h"
#include "RXNEngine/Scene/Entity.h"

namespace RXNEngine {

    SceneRenderer::SceneRenderer(Ref<Scene> scene, const SceneRendererSpecification& spec)
        : m_Scene(scene), m_Specification(spec)
    {
        Init();
    }

    SceneRenderer::~SceneRenderer()
    {
    }

    void SceneRenderer::Init()
    {
        RenderTargetSpecification geoSpec;
        geoSpec.Attachments = { RenderTargetTextureFormat::RGBA16F, RenderTargetTextureFormat::Depth };
        geoSpec.Width = 1280;
        geoSpec.Height = 720;
        m_GeoPass = RenderTarget::Create(geoSpec);

        RenderTargetSpecification finalSpec;
        finalSpec.Attachments = { RenderTargetTextureFormat::RGBA8 };
        finalSpec.Width = 1280;
        finalSpec.Height = 720;
        m_FinalPass = RenderTarget::Create(finalSpec);

        m_PostProcessShader = Shader::Create("assets/shaders/postprocess/screen.glsl");

        float quadVertices[] = {
            -1.0f, -1.0f, 0.0f, 0.0f,  1.0f, -1.0f, 1.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f, -1.0f,  1.0f, 0.0f, 1.0f
        };
        uint32_t quadIndices[] = { 0, 1, 2, 2, 3, 0 };

        m_ScreenQuadVAO = VertexArray::Create();
        Ref<VertexBuffer> vb = VertexBuffer::Create(quadVertices, sizeof(quadVertices));
        vb->SetLayout({ { ShaderDataType::Float2, "a_Position" }, { ShaderDataType::Float2, "a_TexCoord" } });
        m_ScreenQuadVAO->AddVertexBuffer(vb);
        m_ScreenQuadVAO->SetIndexBuffer(IndexBuffer::Create(quadIndices, 6));
    }

    void SceneRenderer::SetViewportSize(uint32_t width, uint32_t height)
    {
        if (m_ViewportWidth != width || m_ViewportHeight != height)
        {
            m_ViewportWidth = width;
            m_ViewportHeight = height;

            m_GeoPass->Resize(width, height);
            m_FinalPass->Resize(width, height);
        }
    }

    void SceneRenderer::RenderEditor(EditorCamera& camera)
    {
        m_GeoPass->Bind();
        RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
        RenderCommand::Clear();

        m_Scene->OnRenderEditor(0.0f, (EditorCamera&)camera, m_GeoPass);

        m_GeoPass->Unbind();

        RenderPostProcess();
    }

    void SceneRenderer::RenderRuntime()
    {
        Entity cameraEntity = m_Scene->GetPrimaryCameraEntity();

        if (!cameraEntity)
            return;

        Camera& camera = cameraEntity.GetComponent<CameraComponent>().Camera;
        glm::mat4 transform = cameraEntity.GetComponent<TransformComponent>().GetTransform();

        m_GeoPass->Bind();
        RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
        RenderCommand::SetDepthTest(true);
        RenderCommand::Clear();


        m_Scene->OnRender(camera, transform, m_GeoPass);

        m_GeoPass->Unbind();

        RenderPostProcess();
    }

    void SceneRenderer::RenderPostProcess()
    {
        m_FinalPass->Bind();

        RenderCommand::SetDepthTest(false);

        RenderCommand::Clear();

        m_PostProcessShader->Bind();
        m_PostProcessShader->SetInt("u_ScreenTexture", 0);
        m_PostProcessShader->SetFloat("u_Exposure", m_Settings.Exposure);
        m_PostProcessShader->SetFloat("u_Gamma", m_Settings.Gamma);

        RenderCommand::BindTextureID(0, m_GeoPass->GetColorAttachmentRendererID());

        m_ScreenQuadVAO->Bind();
        RenderCommand::DrawIndexed(m_ScreenQuadVAO);

        RenderCommand::SetDepthTest(true);

        m_FinalPass->Unbind();
    }
}