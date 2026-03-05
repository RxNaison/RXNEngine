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

        RenderTargetSpecification pickSpec;
        pickSpec.Attachments = { RenderTargetTextureFormat::RED_INTEGER, RenderTargetTextureFormat::Depth };
        pickSpec.Width = 1280;
        pickSpec.Height = 720;
        m_PickingPass = RenderTarget::Create(pickSpec);


        m_PostProcessShader = Shader::Create("assets/shaders/postprocess/screen.glsl");
        m_BloomDownsampleShader = Shader::Create("assets/shaders/postprocess/bloom_downsample.glsl");
        m_BloomUpsampleShader = Shader::Create("assets/shaders/postprocess/bloom_upsample.glsl");
        m_PickingShader = Shader::Create("assets/shaders/editor_picking.glsl");

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
            m_PickingPass->Resize(width, height);

            m_BloomMips.clear();

            glm::vec2 mipSize = { (float)width, (float)height };
            glm::ivec2 mipIntSize = { width, height };

            const uint32_t bloomMipCount = 6;
            for (uint32_t i = 0; i < bloomMipCount; i++)
            {
                mipSize *= 0.5f;
                mipIntSize /= 2;

                if (mipIntSize.x == 0 || mipIntSize.y == 0)
                    break;

                RenderTargetSpecification spec;
                spec.Attachments = { RenderTargetTextureFormat::RGBA16F };
                spec.Width = mipIntSize.x;
                spec.Height = mipIntSize.y;

                m_BloomMips.push_back({ mipSize, RenderTarget::Create(spec) });
            }
        }
    }

    void SceneRenderer::RenderEditor(EditorCamera& camera)
    {
        OPTICK_EVENT();

        m_GeoPass->Bind();
        RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
        RenderCommand::Clear();

        m_Scene->OnRenderEditor(0.0f, (EditorCamera&)camera, m_GeoPass, m_Settings.ShowColliders);

        m_GeoPass->Unbind();

        RenderBloom();
        RenderPostProcess();
    }

    void SceneRenderer::RenderRuntime()
    {
        OPTICK_EVENT();

        Entity cameraEntity = m_Scene->GetPrimaryCameraEntity();

        if (!cameraEntity)
            return;

        Camera& camera = cameraEntity.GetComponent<CameraComponent>().Camera;
        glm::mat4 transform = m_Scene->GetWorldTransform(cameraEntity);

        m_GeoPass->Bind();
        RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
        RenderCommand::SetDepthTest(true);
        RenderCommand::Clear();

        m_Scene->OnRender(camera, transform, m_GeoPass, m_Settings.ShowColliders);

        m_GeoPass->Unbind();

        RenderBloom();
        RenderPostProcess();
    }

    int SceneRenderer::GetEntityIDAtMouse(int x, int y, const EditorCamera& camera)
    {
        m_PickingPass->Bind();
        RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        RenderCommand::Clear();
        m_PickingPass->ClearAttachment(0, -1);

        m_PickingShader->Bind();
        m_PickingShader->SetMat4("u_ViewProjection", camera.GetViewProjection());

        Renderer::ExecutePickingPass(m_PickingShader);

        int pixelData = m_PickingPass->ReadPixel(0, x, y);
        m_PickingPass->Unbind();

        return pixelData;
    }

    void SceneRenderer::RenderPostProcess()
    {
        OPTICK_EVENT();

        m_FinalPass->Bind();

        RenderCommand::SetDepthTest(false);
        RenderCommand::Clear();

        m_PostProcessShader->Bind();
        m_PostProcessShader->SetInt("u_ScreenTexture", 0);
        m_PostProcessShader->SetInt("u_BloomTexture", 1);

        m_PostProcessShader->SetFloat("u_Exposure", m_Settings.Exposure);
        m_PostProcessShader->SetFloat("u_Gamma", m_Settings.Gamma);
        m_PostProcessShader->SetFloat("u_BloomIntensity", m_Settings.BloomIntensity);

        RenderCommand::BindTextureID(0, m_GeoPass->GetColorAttachmentRendererID());

        if (!m_BloomMips.empty())
            RenderCommand::BindTextureID(1, m_BloomMips[0].Target->GetColorAttachmentRendererID());

        m_ScreenQuadVAO->Bind();
        RenderCommand::DrawIndexed(m_ScreenQuadVAO);

        RenderCommand::SetDepthTest(true);

        m_FinalPass->Unbind();
    }

    void SceneRenderer::RenderBloom()
    {
        OPTICK_EVENT();

        if (m_BloomMips.empty()) return;

        RenderCommand::SetDepthTest(false);
        m_ScreenQuadVAO->Bind();

        m_BloomDownsampleShader->Bind();
        m_BloomDownsampleShader->SetInt("u_Texture", 0);

        float knee = m_Settings.BloomThreshold * m_Settings.BloomKnee;
        glm::vec4 filter = {
            m_Settings.BloomThreshold,
            m_Settings.BloomThreshold - knee,
            2.0f * knee,
            0.25f / (knee + 0.00001f)
        };
        m_BloomDownsampleShader->SetFloat3("u_Threshold", filter);

        uint32_t currentTexture = m_GeoPass->GetColorAttachmentRendererID();

        for (uint32_t i = 0; i < m_BloomMips.size(); i++)
        {
            auto& mip = m_BloomMips[i];

            mip.Target->Bind();
            RenderCommand::SetViewport(0, 0, mip.Size.x, mip.Size.y);

            m_BloomDownsampleShader->SetInt("u_MipLevel", i);
            m_BloomDownsampleShader->SetFloat2("u_TexelSize", glm::vec2(1.0f / mip.Size.x, 1.0f / mip.Size.y));
            RenderCommand::BindTextureID(0, currentTexture);

            RenderCommand::DrawIndexed(m_ScreenQuadVAO);

            currentTexture = mip.Target->GetColorAttachmentRendererID();
            mip.Target->Unbind();
        }

        m_BloomUpsampleShader->Bind();
        m_BloomUpsampleShader->SetInt("u_Texture", 0);
        m_BloomUpsampleShader->SetFloat("u_FilterRadius", m_Settings.BloomFilterRadius);

        RenderCommand::SetBlend(true);
        RenderCommand::SetBlendFunc(RendererAPI::BlendFactor::One, RendererAPI::BlendFactor::One);
        RenderCommand::SetBlendEquation(RendererAPI::BlendEquation::Add);

        for (int i = m_BloomMips.size() - 1; i > 0; i--)
        {
            auto& currentMip = m_BloomMips[i];
            auto& nextMip = m_BloomMips[i - 1];

            nextMip.Target->Bind();
            RenderCommand::SetViewport(0, 0, nextMip.Size.x, nextMip.Size.y);

            RenderCommand::BindTextureID(0, currentMip.Target->GetColorAttachmentRendererID());

            RenderCommand::DrawIndexed(m_ScreenQuadVAO);

            nextMip.Target->Unbind();
        }

        RenderCommand::SetBlendFunc(RendererAPI::BlendFactor::SrcAlpha, RendererAPI::BlendFactor::OneMinusSrcAlpha);
        RenderCommand::SetDepthTest(true);
    }
}