#include "rxnpch.h"
#include "SceneRenderer.h"
#include "RXNEngine/UI/UISystem.h"
#include "RXNEngine/Renderer/Renderer.h"
#include "RXNEngine/Renderer/RenderCommand.h"
#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Scene/Components.h"
#include "RXNEngine/Renderer/Renderer2D.h"
#include "RXNEngine/Core/Application.h"
#include "RXNEngine/Core/JobSystem.h"
#include "RXNEngine/Physics/PhysicsWorld.h"
#include "RXNEngine/Asset/AssetManager.h"
#include "RXNEngine/Math/Frustum.h"
#include "RXNEngine/Renderer/GPUProfiler.h"
#include <atomic>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cctype>
#include <limits>

namespace RXNEngine {

    namespace PhysicsUtils {
        inline glm::vec3 PhysXToGLM(const physx::PxVec3& vec) { return { vec.x, vec.y, vec.z }; }
        inline glm::quat PhysXToGLM(const physx::PxQuat& q) { return { q.w, q.x, q.y, q.z }; }
        static glm::vec4 UnpackPhysXColor(physx::PxU32 color)
        {
            float b = (float)((color >> 16) & 0xFF) / 255.0f;
            float g = (float)((color >> 8) & 0xFF) / 255.0f;
            float r = (float)((color >> 0) & 0xFF) / 255.0f;
            return { r, g, b, 1.0f };
        }
    }

    namespace {
        static bool PathsMatch(const std::string& path1, const std::string& path2)
        {
            if (path1.empty() || path2.empty())
                return false;

            auto getFilename = [](const std::string& path) -> std::string {
                size_t lastSlash = path.find_last_of("/\\");
                std::string filename = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
                std::transform(filename.begin(), filename.end(), filename.begin(), [](unsigned char c) {
                    return std::tolower(c);
                });
                return filename;
            };

            return getFilename(path1) == getFilename(path2);
        }
    }

    struct RenderCommandData
    {
        Ref<StaticMesh> Mesh;
        uint32_t SubmeshIndex;
        Ref<Material> Material;
        glm::mat4 Transform;
        int EntityID;
        bool IsVisibleToCamera;
        bool IsVisibleToShadows;
        glm::vec3 BoundingCenter;
        float BoundingRadius;
        bool IsDynamic;
        uint32_t LODIndex;
    };

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
        uint32_t width = m_ViewportWidth > 0 ? m_ViewportWidth : 1;
        uint32_t height = m_ViewportHeight > 0 ? m_ViewportHeight : 1;

        RenderTargetSpecification geoSpec;
        geoSpec.Attachments = { RenderTargetTextureFormat::RGBA16F, RenderTargetTextureFormat::RGBA16F, RenderTargetTextureFormat::RGBA16F, RenderTargetTextureFormat::Depth };
        geoSpec.Width = width;
        geoSpec.Height = height;
        geoSpec.Samples = m_Specification.DisableMSAA ? 1 : 4;
        m_GeoPass = RenderTarget::Create(geoSpec);

        RenderTargetSpecification resolveSpec;
        resolveSpec.Attachments = { RenderTargetTextureFormat::RGBA16F, RenderTargetTextureFormat::RGBA16F, RenderTargetTextureFormat::RGBA16F, RenderTargetTextureFormat::Depth };
        resolveSpec.Width = width;
        resolveSpec.Height = height;
        resolveSpec.Samples = 1;
        m_ResolvePass = RenderTarget::Create(resolveSpec);
        m_PrevResolvePass = RenderTarget::Create(resolveSpec);

        m_HiZ = HiZBuffer::Create();
        m_HiZ->Init(width, height);

        RenderTargetSpecification finalSpec;
        finalSpec.Attachments = { RenderTargetTextureFormat::RGBA8 };
        finalSpec.Width = width;
        finalSpec.Height = height;
        m_FinalPass = RenderTarget::Create(finalSpec);

        if (!m_Specification.DisablePicking)
        {
            RenderTargetSpecification pickSpec;
            pickSpec.Attachments = { RenderTargetTextureFormat::RED_INTEGER, RenderTargetTextureFormat::Depth };
            pickSpec.Width = width;
            pickSpec.Height = height;
            m_PickingPass = RenderTarget::Create(pickSpec);
        }
        else
        {
            m_PickingPass = nullptr;
        }

        if (!m_Specification.DisableOutline)
        {
            RenderTargetSpecification maskSpec;
            maskSpec.Attachments = { RenderTargetTextureFormat::RGBA8 };
            maskSpec.Width = width;
            maskSpec.Height = height;
            m_OutlineMaskPass = RenderTarget::Create(maskSpec);
        }
        else
        {
            m_OutlineMaskPass = nullptr;
        }

        if (!m_Specification.DisableAO)
        {
            RenderTargetSpecification aoSpec;
            aoSpec.Attachments = { RenderTargetTextureFormat::RGBA16F };
            aoSpec.Width = (width + 1) / 2; 
            aoSpec.Height = (height + 1) / 2;
            aoSpec.Samples = 1;
            m_AOPass = RenderTarget::Create(aoSpec);
            m_AOBlurPass = RenderTarget::Create(aoSpec);
        }
        else
        {
            m_AOPass = nullptr;
            m_AOBlurPass = nullptr;
        }

        m_PostProcessShader = Shader::Create("res/shaders/postprocess/screen.glsl");
        m_BloomDownsampleShader = Shader::Create("res/shaders/postprocess/bloom_downsample.glsl");
        m_BloomUpsampleShader = Shader::Create("res/shaders/postprocess/bloom_upsample.glsl");
        m_PickingShader = Shader::Create("res/shaders/editor_picking.glsl");
        m_UIPickingShader = Shader::Create("res/shaders/editor_ui_picking.glsl");
        m_GridShader = Shader::Create("res/shaders/grid.glsl");
        m_OutlineMaskShader = Shader::Create("res/shaders/outline_mask.glsl");

        m_AOShader = Shader::Create("res/shaders/postprocess/gtao.glsl");
        m_AOBlurShader = Shader::Create("res/shaders/postprocess/ao_blur.glsl");

        {
            RenderTargetSpecification ssrSpec;
            ssrSpec.Attachments = { RenderTargetTextureFormat::RGBA16F };
            ssrSpec.Width = width;
            ssrSpec.Height = height;
            ssrSpec.Samples = 1;
            m_SSRBlurPass = RenderTarget::Create(ssrSpec);
            m_SSRPass = RenderTarget::Create(ssrSpec);
        }
        m_SSRDenoiseShader = Shader::Create("res/shaders/postprocess/ssr_denoise.glsl");

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


        float gridVertices[] = {
            -1.0f, 0.0f, -1.0f,
             1.0f, 0.0f, -1.0f,
             1.0f, 0.0f,  1.0f,
            -1.0f, 0.0f,  1.0f
        };
        uint32_t gridIndices[] = { 0, 1, 2, 2, 3, 0 };
        m_GridQuadVAO = VertexArray::Create();
        Ref<VertexBuffer> gridVB = VertexBuffer::Create(gridVertices, sizeof(gridVertices));
        gridVB->SetLayout({ { ShaderDataType::Float3, "a_Position" } });
        m_GridQuadVAO->AddVertexBuffer(gridVB);
        m_GridQuadVAO->SetIndexBuffer(IndexBuffer::Create(gridIndices, 6));

        float uiQuadVertices[] = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.5f,  0.5f, 0.0f,
            -0.5f,  0.5f, 0.0f
        };
        uint32_t uiQuadIndices[] = { 0, 1, 2, 2, 3, 0 };
        m_UIPickingQuadVAO = VertexArray::Create();
        Ref<VertexBuffer> uiVB = VertexBuffer::Create(uiQuadVertices, sizeof(uiQuadVertices));
        uiVB->SetLayout({ { ShaderDataType::Float3, "a_Position" } });
        m_UIPickingQuadVAO->AddVertexBuffer(uiVB);
        m_UIPickingQuadVAO->SetIndexBuffer(IndexBuffer::Create(uiQuadIndices, 6));
    }

    void SceneRenderer::SetViewportSize(uint32_t width, uint32_t height)
    {
        if (m_ViewportWidth != width || m_ViewportHeight != height)
        {
            m_ViewportWidth = width;
            m_ViewportHeight = height;

            m_GeoPass->Resize(width, height);
            m_ResolvePass->Resize(width, height);
            m_PrevResolvePass->Resize(width, height);
            if (m_HiZ) m_HiZ->Resize(width, height);
            m_FinalPass->Resize(width, height);
            if (m_PickingPass) m_PickingPass->Resize(width, height);
            if (m_OutlineMaskPass) m_OutlineMaskPass->Resize(width, height);

            if (m_AOPass && m_AOBlurPass)
            {
                uint32_t aoWidth = (width + 1) / 2;
                uint32_t aoHeight = (height + 1) / 2;
                m_AOPass->Resize(aoWidth, aoHeight);
                m_AOBlurPass->Resize(aoWidth, aoHeight);
            }

            if (m_SSRBlurPass) m_SSRBlurPass->Resize(width, height);
            if (m_SSRPass) m_SSRPass->Resize(width, height);

            m_Scene->OnViewportResize(width, height);

            m_BloomMips.clear();

            if (!m_Specification.DisableBloom)
            {
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
    }

    void SceneRenderer::RenderEditor(uint32_t targetWidth, uint32_t targetHeight, float deltaTime, EditorCamera& camera, const std::vector<Entity>& selectedEntities)
    {
        RXN_PROFILE_SCOPE();

        GPUProfiler::Get().BeginFrame();

        std::swap(m_ResolvePass, m_PrevResolvePass);
        {
            RXN_GPU_SCOPE("PrevFrame Mipmaps");
            m_PrevResolvePass->GenerateMipmaps(0);
        }

        if (targetWidth > 0 && targetHeight > 0 && (m_ViewportWidth != targetWidth || m_ViewportHeight != targetHeight))
        {
            SetViewportSize(targetWidth, targetHeight);
            camera.SetViewportSize((float)targetWidth, (float)targetHeight);
        }

        if (m_OutlineMaskPass)
        {
            m_OutlineMaskPass->Bind();
            RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 0.0f });
            RenderCommand::Clear();
        }

        bool showScreenSpaceUI = false;

        if (m_OutlineMaskPass && !selectedEntities.empty())
        {
            RenderCommand::SetDepthTest(false);

            m_OutlineMaskShader->Bind();
            m_OutlineMaskShader->SetMat4("u_ViewProjection", camera.GetViewProjection());

            auto drawOutline = [&](Entity entity, auto& drawOutlineRef) -> void
                {
                    if (entity.HasComponent<StaticMeshComponent>())
                    {
                        auto& mc = entity.GetComponent<StaticMeshComponent>();
                        if (mc.Mesh)
                        {
                            glm::mat4 transform = m_Scene->GetWorldTransform(entity);
                            Entity animatorParent;
                            if (entity.HasComponent<RelationshipComponent>())
                            {
                                auto& rc = entity.GetComponent<RelationshipComponent>();
                                UUID parentHandle = rc.ParentHandle;
                                while (parentHandle != 0)
                                {
                                    Entity parent = m_Scene->GetEntityByUUID(parentHandle);
                                    if (parent)
                                    {
                                        if (parent.HasComponent<AnimatorComponent>())
                                        {
                                            auto& animator = parent.GetComponent<AnimatorComponent>();
                                            if (animator.SkinnedVAO && animator.MeshAsset)
                                            {
                                                if (PathsMatch(mc.AssetPath, animator.MeshAssetPath))
                                                {
                                                    animatorParent = parent;
                                                }
                                            }
                                            break;
                                        }
                                        if (parent.HasComponent<RelationshipComponent>())
                                        {
                                            parentHandle = parent.GetComponent<RelationshipComponent>().ParentHandle;
                                        }
                                        else
                                        {
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        break;
                                    }
                                }
                            }

                            if (animatorParent)
                            {
                                transform = m_Scene->GetWorldTransform(animatorParent);
                            }

                            Application::Get().GetSubsystem<Renderer>()->DrawEntityOutline(mc.Mesh, mc.SubmeshIndex, transform, m_OutlineMaskShader, m_Scene.get(), (int)(uint32_t)entity);
                        }
                    }

                    if (entity.HasComponent<RelationshipComponent>())
                    {
                        for (UUID childID : entity.GetComponent<RelationshipComponent>().Children)
                        {
                            Entity child = m_Scene->GetEntityByUUID(childID);

                            if (child)
                                drawOutlineRef(child, drawOutlineRef);
                        }
                    }
                };

            for (Entity entity : selectedEntities)
            {
                if (entity)
                {
                    drawOutline(entity, drawOutline);

                    if (entity.HasComponent<UITransformComponent>() || entity.HasComponent<UICanvasComponent>())
                        showScreenSpaceUI = true;
                }
            }

            Renderer2D::BeginScene(camera.GetViewProjection());

            auto drawUIOutline = [&](Entity entity, auto& drawUIOutlineRef) -> void
                {
                    if (entity.HasComponent<UITransformComponent>())
                    {
                        bool isWorldSpace = true;
                        Entity curr = entity;
                        while (curr)
                        {
                            if (curr.HasComponent<UICanvasComponent>())
                            {
                                isWorldSpace = curr.GetComponent<UICanvasComponent>().RenderMode == CanvasRenderMode::WorldSpace;
                                break;
                            }
                            if (curr.HasComponent<RelationshipComponent>())
                            {
                                UUID pid = curr.GetComponent<RelationshipComponent>().ParentHandle;
                                curr = pid != 0 ? m_Scene->GetEntityByUUID(pid) : Entity{};
                            }
                            else break;
                        }

                        if (isWorldSpace)
                            Renderer2D::DrawQuad(entity.GetComponent<UITransformComponent>().ComputedTransform, glm::vec4(1.0f));
                    }

                    if (entity.HasComponent<RelationshipComponent>())
                    {
                        for (UUID childID : entity.GetComponent<RelationshipComponent>().Children)
                        {
                            Entity child = m_Scene->GetEntityByUUID(childID);

                            if (child)
                                drawUIOutlineRef(child, drawUIOutlineRef);
                        }
                    }
                };

            for (Entity entity : selectedEntities)
            {
                if (entity)
                    drawUIOutline(entity, drawUIOutline);
            }

            Renderer2D::EndScene();

            RenderCommand::SetDepthTest(true);
        }
        if (m_OutlineMaskPass)
            m_OutlineMaskPass->Unbind();

        m_GeoPass->Bind();
        RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
        RenderCommand::Clear();
        m_GeoPass->ClearColorAttachmentFloat(1, 0.0f, 0.0f, 0.0f, 0.0f);
        m_GeoPass->ClearColorAttachmentFloat(2, 0.5f, 0.5f, 1.0f, 0.0f);

        m_Scene->UpdateWorldTransforms();
        m_Scene->GetSubsystem<UISystem>()->Update(deltaTime);

        CaptureReflectionProbes();
        m_GeoPass->Bind();

        glm::mat4 cameraTransform = glm::inverse(camera.GetViewMatrix());
        RenderScene(camera, cameraTransform, GetSettings().ShowColliders, deltaTime, selectedEntities);

        m_GeoPass->Bind();
        RenderCommand::SetBlend(true);
        glm::mat4 editorCameraTransform = glm::inverse(camera.GetViewMatrix());
        RenderUI(camera, editorCameraTransform, true);   // World Space

        if (showScreenSpaceUI)
            RenderUI(camera, editorCameraTransform, false, selectedEntities);  // Screen Space

        m_GeoPass->Unbind();

        ResolveMSAA();
        RenderAO(camera, cameraTransform);
        RenderBloom();
        RenderSSRDenoise();
        RenderPostProcess();

        RenderGridOverlay(camera);

        GPUProfiler::Get().EndFrame();
    }

    void SceneRenderer::RenderToTarget(uint32_t width, uint32_t height, Camera& camera, const glm::mat4& cameraTransform)
    {
        RXN_PROFILE_SCOPE();

        std::swap(m_ResolvePass, m_PrevResolvePass);
        m_PrevResolvePass->GenerateMipmaps(0);

        SetViewportSize(width, height);

        if (m_OutlineMaskPass)
        {
            m_OutlineMaskPass->Bind();
            RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 0.0f });
            RenderCommand::Clear();
            m_OutlineMaskPass->Unbind();
        }

        CaptureReflectionProbes();

        m_GeoPass->Bind();
        RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
        RenderCommand::SetDepthTest(true);
        RenderCommand::Clear();
        m_GeoPass->ClearColorAttachmentFloat(1, 0.0f, 0.0f, 0.0f, 0.0f);
        m_GeoPass->ClearColorAttachmentFloat(2, 0.5f, 0.5f, 1.0f, 0.0f);

        RenderScene(camera, cameraTransform, false);

        m_GeoPass->Unbind();

        ResolveMSAA();
        RenderAO(camera, cameraTransform);
        RenderBloom();
        RenderSSRDenoise();
        RenderPostProcess();
    }

    int SceneRenderer::GetEntityIDAtMouse(int x, int y, const EditorCamera& camera, const std::vector<Entity>& selectedEntities)
    {
        m_PickingPass->Bind();
        RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        RenderCommand::Clear();
        m_PickingPass->ClearAttachment(0, -1);

        m_PickingShader->Bind();
        m_PickingShader->SetMat4("u_ViewProjection", camera.GetViewProjection());

        Application::Get().GetSubsystem<Renderer>()->ExecutePickingPass(m_PickingShader);

        RenderUIPicking(camera, selectedEntities);

        int pixelData = m_PickingPass->ReadPixel(0, x, y);
        m_PickingPass->Unbind();

        return pixelData;
    }

    void SceneRenderer::ResolveMSAA()
    {
        RXN_PROFILE_SCOPE();
        RXN_GPU_SCOPE("MSAA Resolve");

        bool scissorEnabled = RenderCommand::IsScissorTestEnabled();
        if (scissorEnabled)
            RenderCommand::SetScissorTest(false);

        RenderCommand::BlitRenderTarget(m_GeoPass, m_ResolvePass);

        if (scissorEnabled)
            RenderCommand::SetScissorTest(true);
    }

    void SceneRenderer::RenderAO(const Camera& camera, const glm::mat4& cameraTransform)
    {
        RXN_GPU_SCOPE("GTAO + Blur");
        if (!m_AOPass || !m_AOBlurPass)
            return;

        RXN_PROFILE_SCOPE();

        uint32_t w = m_ViewportWidth > 0 ? m_ViewportWidth : 1;
        uint32_t h = m_ViewportHeight > 0 ? m_ViewportHeight : 1;
        uint32_t aoW = (w + 1) / 2; 
        uint32_t aoH = (h + 1) / 2;

        bool scissorEnabled = RenderCommand::IsScissorTestEnabled();
        if (scissorEnabled)
            RenderCommand::SetScissorTest(false);

        RenderCommand::SetDepthTest(false);
        RenderCommand::SetBlend(false);
        m_ScreenQuadVAO->Bind();

        m_AOPass->Bind();
        RenderCommand::SetViewport(0, 0, aoW, aoH);
        RenderCommand::SetClearColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        RenderCommand::Clear();

        glm::mat4 view = glm::inverse(cameraTransform);
        glm::mat4 proj = camera.GetProjection();
        glm::mat4 invProj = glm::inverse(proj);

        m_AOShader->Bind();
        m_AOShader->SetInt("u_DepthTexture", 0);
        m_AOShader->SetInt("u_NormalTexture", 1);
        m_AOShader->SetMat4("u_View", view);
        m_AOShader->SetMat4("u_Proj", proj);
        m_AOShader->SetMat4("u_InvProj", invProj);
        m_AOShader->SetFloat2("u_AOTexelSize", glm::vec2(1.0f / (float)aoW, 1.0f / (float)aoH));
        m_AOShader->SetFloat2("u_Resolution", glm::vec2((float)aoW, (float)aoH));
        m_AOShader->SetFloat("u_Radius", 0.8f);
        m_AOShader->SetFloat("u_Intensity", 1.4f);
        m_AOShader->SetFloat("u_FalloffMul", 0.5f);

        RenderCommand::BindTextureID(0, m_ResolvePass->GetDepthAttachmentRendererID());
        RenderCommand::BindTextureID(1, m_ResolvePass->GetColorAttachmentRendererID(2));

        RenderCommand::DrawIndexed(m_ScreenQuadVAO);
        m_AOPass->Unbind();

        m_AOBlurPass->Bind();
        RenderCommand::SetViewport(0, 0, aoW, aoH);
        RenderCommand::SetClearColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        RenderCommand::Clear();

        m_AOBlurShader->Bind();
        m_AOBlurShader->SetInt("u_AOTexture", 0);
        m_AOBlurShader->SetInt("u_DepthTexture", 1);
        m_AOBlurShader->SetInt("u_NormalTexture", 2);
        m_AOBlurShader->SetMat4("u_InvProj", invProj);
        m_AOBlurShader->SetFloat2("u_TexelSize", glm::vec2(1.0f / (float)aoW, 1.0f / (float)aoH));

        RenderCommand::BindTextureID(0, m_AOPass->GetColorAttachmentRendererID());
        RenderCommand::BindTextureID(1, m_ResolvePass->GetDepthAttachmentRendererID());
        RenderCommand::BindTextureID(2, m_ResolvePass->GetColorAttachmentRendererID(2));

        RenderCommand::DrawIndexed(m_ScreenQuadVAO);
        m_AOBlurPass->Unbind();

        RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });

        RenderCommand::SetDepthTest(true);
        RenderCommand::SetBlend(true);

        if (scissorEnabled)
            RenderCommand::SetScissorTest(true);
    }

    void SceneRenderer::RenderSSRDenoise()
    {
        RXN_GPU_SCOPE("SSR Denoise");
        if (!m_SSRBlurPass || !m_SSRPass || !m_SSRDenoiseShader)
            return;

        RXN_PROFILE_SCOPE();

        uint32_t w = m_ViewportWidth > 0 ? m_ViewportWidth : 1;
        uint32_t h = m_ViewportHeight > 0 ? m_ViewportHeight : 1;
        glm::vec2 texel = glm::vec2(1.0f / (float)w, 1.0f / (float)h);

        bool scissorEnabled = RenderCommand::IsScissorTestEnabled();
        if (scissorEnabled)
            RenderCommand::SetScissorTest(false);

        RenderCommand::SetDepthTest(false);
        RenderCommand::SetBlend(false);
        RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        m_ScreenQuadVAO->Bind();

        m_SSRDenoiseShader->Bind();
        m_SSRDenoiseShader->SetInt("u_SSRTexture", 0);
        m_SSRDenoiseShader->SetInt("u_DepthTexture", 1);
        m_SSRDenoiseShader->SetMat4("u_InverseViewProjection", glm::inverse(m_CurrentViewProjection));
        m_SSRDenoiseShader->SetFloat2("u_TexelSize", texel);

        m_SSRBlurPass->Bind();
        RenderCommand::SetViewport(0, 0, w, h);
        RenderCommand::Clear();
        m_SSRDenoiseShader->SetFloat2("u_Direction", glm::vec2(1.0f, 0.0f));
        RenderCommand::BindTextureID(0, m_ResolvePass->GetColorAttachmentRendererID(1));
        RenderCommand::BindTextureID(1, m_ResolvePass->GetDepthAttachmentRendererID());
        RenderCommand::DrawIndexed(m_ScreenQuadVAO);
        m_SSRBlurPass->Unbind();

        m_SSRPass->Bind();
        RenderCommand::SetViewport(0, 0, w, h);
        RenderCommand::Clear();
        m_SSRDenoiseShader->SetFloat2("u_Direction", glm::vec2(0.0f, 1.0f));
        RenderCommand::BindTextureID(0, m_SSRBlurPass->GetColorAttachmentRendererID());
        RenderCommand::BindTextureID(1, m_ResolvePass->GetDepthAttachmentRendererID());
        RenderCommand::DrawIndexed(m_ScreenQuadVAO);
        m_SSRPass->Unbind();

        RenderCommand::SetDepthTest(true);
        RenderCommand::SetBlend(true);

        if (scissorEnabled)
            RenderCommand::SetScissorTest(true);
    }

    void SceneRenderer::RenderPostProcess()
    {
        RXN_GPU_SCOPE("Post-process");
        RXN_PROFILE_SCOPE();

        bool scissorEnabled = RenderCommand::IsScissorTestEnabled();
        if (scissorEnabled)
            RenderCommand::SetScissorTest(false);

        m_FinalPass->Bind();

        RenderCommand::SetDepthTest(false);
        RenderCommand::Clear();

        m_PostProcessShader->Bind();
        m_PostProcessShader->SetInt("u_ScreenTexture", 0);
        m_PostProcessShader->SetInt("u_BloomTexture", 1);
        m_PostProcessShader->SetInt("u_OutlineTexture", 2);
        m_PostProcessShader->SetInt("u_DepthTexture", 3);
        m_PostProcessShader->SetInt("u_AOTexture", 4);
        m_PostProcessShader->SetInt("u_SSRTexture", 5);

        m_PostProcessShader->SetMat4("u_InverseViewProjection", glm::inverse(m_CurrentViewProjection));
        
        const auto& spec = m_FinalPass->GetSpecification();
        m_PostProcessShader->SetFloat2("u_TexelSize", glm::vec2(1.0f / spec.Width, 1.0f / spec.Height));

        uint32_t aoTexW = ((m_ViewportWidth > 0 ? m_ViewportWidth : 1) + 1) / 2;
        uint32_t aoTexH = ((m_ViewportHeight > 0 ? m_ViewportHeight : 1) + 1) / 2;
        m_PostProcessShader->SetFloat2("u_AOTexelSize", glm::vec2(1.0f / (float)aoTexW, 1.0f / (float)aoTexH));

        m_PostProcessShader->SetFloat("u_Exposure", GetSettings().Exposure);
        m_PostProcessShader->SetFloat("u_Gamma", GetSettings().Gamma);
        m_PostProcessShader->SetFloat("u_BloomIntensity", GetSettings().BloomIntensity);

        RenderCommand::BindTextureID(0, m_ResolvePass->GetColorAttachmentRendererID());

        if (!m_BloomMips.empty())
            RenderCommand::BindTextureID(1, m_BloomMips[0].Target->GetColorAttachmentRendererID());
        else
            RenderCommand::BindTextureID(1, Texture2D::BlackTexture()->GetRendererID());

        RenderCommand::BindTextureID(2, m_OutlineMaskPass ? m_OutlineMaskPass->GetColorAttachmentRendererID() : Texture2D::BlackTexture()->GetRendererID());
        RenderCommand::BindTextureID(3, m_ResolvePass->GetDepthAttachmentRendererID());
        RenderCommand::BindTextureID(4, m_AOBlurPass ? m_AOBlurPass->GetColorAttachmentRendererID() : Texture2D::WhiteTexture()->GetRendererID());
        RenderCommand::BindTextureID(5, m_SSRPass ? m_SSRPass->GetColorAttachmentRendererID() : Texture2D::BlackTexture()->GetRendererID());

        m_ScreenQuadVAO->Bind();
        RenderCommand::DrawIndexed(m_ScreenQuadVAO);

        RenderCommand::SetDepthTest(true);

        m_FinalPass->Unbind();

        if (scissorEnabled)
            RenderCommand::SetScissorTest(true);
    }

    void SceneRenderer::RenderBloom()
    {
        RXN_GPU_SCOPE("Bloom");
        RXN_PROFILE_SCOPE();

        if (m_BloomMips.empty())
            return;

        bool scissorEnabled = RenderCommand::IsScissorTestEnabled();
        if (scissorEnabled)
            RenderCommand::SetScissorTest(false);

        RenderCommand::SetDepthTest(false);
        m_ScreenQuadVAO->Bind();

        m_BloomDownsampleShader->Bind();
        m_BloomDownsampleShader->SetInt("u_Texture", 0);

        float knee = GetSettings().BloomThreshold * GetSettings().BloomKnee;
        glm::vec4 filter = {
            GetSettings().BloomThreshold,
            GetSettings().BloomThreshold - knee,
            2.0f * knee,
            0.25f / (knee + 0.00001f)
        };
        m_BloomDownsampleShader->SetFloat4("u_Threshold", filter);

        uint32_t currentTexture = m_ResolvePass->GetColorAttachmentRendererID();

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
        m_BloomUpsampleShader->SetFloat("u_FilterRadius", GetSettings().BloomFilterRadius);

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

        if (scissorEnabled)
            RenderCommand::SetScissorTest(true);
    }


    void SceneRenderer::CaptureReflectionProbes()
    {
        RXN_GPU_SCOPE("Reflection Probes");
        if (!m_Scene)
            return;

        auto view = m_Scene->m_Registry.view<ReflectionProbeComponent, TransformComponent>();

        bool anyActive = false;
        for (auto entity : view)
        {
            if (view.get<ReflectionProbeComponent>(entity).Active) { anyActive = true; break; }
        }
        if (!anyActive)
            return;

        const uint32_t res = 128;
        const uint32_t maxProbes = 4;

        if (!m_ProbeStorageReady)
        {
            RenderTargetSpecification spec;
            spec.Width = (float)res;
            spec.Height = (float)res;
            spec.Attachments = { RenderTargetTextureFormat::RGBA16F, RenderTargetTextureFormat::Depth };
            spec.Samples = 1;
            m_ProbeCapturePass = RenderTarget::Create(spec);

            m_ProbeCache = ReflectionProbeCache::Create();
            m_ProbeCache->Init(res, maxProbes);
            m_ProbeStorageReady = true;
        }

        static const glm::vec3 faceDirs[6] = {
            {  1.0f,  0.0f,  0.0f }, { -1.0f,  0.0f,  0.0f },
            {  0.0f,  1.0f,  0.0f }, {  0.0f, -1.0f,  0.0f },
            {  0.0f,  0.0f,  1.0f }, {  0.0f,  0.0f, -1.0f }
        };
        static const glm::vec3 faceUps[6] = {
            {  0.0f, -1.0f,  0.0f }, {  0.0f, -1.0f,  0.0f },
            {  0.0f,  0.0f,  1.0f }, {  0.0f,  0.0f, -1.0f },
            {  0.0f, -1.0f,  0.0f }, {  0.0f, -1.0f,  0.0f }
        };
        Camera faceCamera(glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f));

        uint32_t index = 0;
        bool capturedThisFrame = false;
        for (auto entity : view)
        {
            auto& pc = view.get<ReflectionProbeComponent>(entity);
            if (!pc.Active)
                continue;
            if (index >= maxProbes)
                break;

            glm::mat4 worldTransform = m_Scene->GetWorldTransform({ entity, m_Scene.get() });
            glm::vec3 probePos = glm::vec3(worldTransform[3]);

            if (pc.Captured && glm::distance(probePos, pc.LastCapturePosition) > 0.05f)
                pc.Dirty = true;

            if (pc.Dirty && !capturedThisFrame)
            {
                RXN_PROFILE_SCOPE_NAMED("Probe Capture Face");

                uint32_t face = pc.CaptureFaceIndex;
                glm::mat4 viewMatrix = glm::lookAt(probePos, probePos + faceDirs[face], faceUps[face]);
                glm::mat4 faceTransform = glm::inverse(viewMatrix);

                m_ProbeCapturePass->Bind();
                RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 1.0f });
                RenderCommand::SetDepthTest(true);
                RenderCommand::Clear();

                RenderScene(faceCamera, faceTransform, false, 0.0f, {}, m_ProbeCapturePass);

                m_ProbeCapturePass->Unbind();
                m_ProbeCache->IngestFace(face, m_ProbeCapturePass->GetColorAttachmentRendererID());

                pc.CaptureFaceIndex++;
                if (pc.CaptureFaceIndex >= 6)
                {
                    m_ProbeCache->Prefilter(index);
                    pc.CaptureFaceIndex = 0;
                    pc.Dirty = false;
                    pc.Captured = true;
                    pc.LastCapturePosition = probePos;
                }
                capturedThisFrame = true;
            }

            ++index;
        }

        uint32_t globalPrefilter = m_Scene->m_Skybox ? m_Scene->m_Skybox->GetPrefilterRendererID() : 0;
        index = 0;
        for (auto entity : view)
        {
            auto& pc = view.get<ReflectionProbeComponent>(entity);
            if (!pc.Active)
                continue;
            if (index >= maxProbes)
                break;

            if (pc.Captured)
                m_ProbeCache->BindPrefiltered(index, 26 + index);
            else if (globalPrefilter != 0)
                RenderCommand::BindTextureID(26 + index, globalPrefilter);

            ++index;
        }
    }

    struct GBufferMaskGuard
    {
        GBufferMaskGuard()
        {
            RenderCommand::SetColorMaskIndexed(1, false, false, false, false);
            RenderCommand::SetColorMaskIndexed(2, false, false, false, false);
        }
        ~GBufferMaskGuard()
        {
            RenderCommand::SetColorMaskIndexed(1, true, true, true, true);
            RenderCommand::SetColorMaskIndexed(2, true, true, true, true);
        }
    };

    static std::atomic<uint32_t> s_HiZOccludedCount{ 0 };

    static bool ComputeOcclusionRect(const AABB& worldAABB, const glm::mat4& viewProj, glm::vec2& outUVMin, glm::vec2& outUVMax, float& outNearestDepth)
    {
        glm::vec2 uvMin(std::numeric_limits<float>::max());
        glm::vec2 uvMax(-std::numeric_limits<float>::max());
        float nearestDepth = std::numeric_limits<float>::max();

        for (int i = 0; i < 8; i++)
        {
            glm::vec3 corner(
                (i & 1) ? worldAABB.Max.x : worldAABB.Min.x,
                (i & 2) ? worldAABB.Max.y : worldAABB.Min.y,
                (i & 4) ? worldAABB.Max.z : worldAABB.Min.z);

            glm::vec4 clip = viewProj * glm::vec4(corner, 1.0f);
            if (clip.w <= 0.1f)
                return false;

            glm::vec2 uv = (glm::vec2(clip.x, clip.y) / clip.w) * 0.5f + 0.5f;
            uvMin = glm::min(uvMin, uv);
            uvMax = glm::max(uvMax, uv);
            nearestDepth = glm::min(nearestDepth, clip.w);
        }

        uvMin = glm::clamp(uvMin, glm::vec2(0.0f), glm::vec2(1.0f));
        uvMax = glm::clamp(uvMax, glm::vec2(0.0f), glm::vec2(1.0f));

        if (uvMax.x <= uvMin.x || uvMax.y <= uvMin.y)
            return false;

        outUVMin = uvMin;
        outUVMax = uvMax;
        outNearestDepth = nearestDepth;
        return true;
    }

    bool SceneRenderer::RasterizeOccluders(const glm::mat4& viewProj, const Frustum& frustum, const glm::vec3& cameraPos)
    {
        RXN_PROFILE_SCOPE();

        m_SoftOcclusion.BeginFrame(viewProj);

        constexpr float kMinOccluderRadius = 1.5f;
        constexpr uint32_t kMaxOccluderTriangles = 30000;

        struct Candidate
        {
            Ref<StaticMesh> Mesh;
            uint32_t SubmeshIndex;
            glm::mat4 Transform;
            float Score;
        };
        std::vector<Candidate> candidates;
        candidates.reserve(64);

        auto view = m_Scene->m_Registry.view<StaticMeshComponent, TransformComponent>();
        for (auto e : view)
        {
            auto [mc, tc] = view.get<StaticMeshComponent, TransformComponent>(e);
            if (!mc.Mesh || mc.SubmeshIndex >= mc.Mesh->GetSubmeshes().size())
                continue;

            if (m_Scene->m_Registry.all_of<AnimatorComponent>(e))
                continue;

            const auto& submesh = mc.Mesh->GetSubmeshes()[mc.SubmeshIndex];

            if (mc.MaterialTableOverride)
            {
                if (mc.MaterialTableOverride->IsTransparent())
                    continue;
            }
            else
            {
                const auto& materials = mc.Mesh->GetMaterials();
                if (submesh.MaterialIndex < materials.size())
                {
                    const auto& mat = materials[submesh.MaterialIndex];
                    if (mat && mat->IsTransparent())
                        continue;
                }
            }

            glm::mat4 transform = tc.WorldTransform;
            AABB worldAABB = Math::CalculateWorldAABB(submesh.BoundingBox, transform);
            glm::vec3 center = (worldAABB.Min + worldAABB.Max) * 0.5f;
            float radius = glm::distance(worldAABB.Min, worldAABB.Max) * 0.5f;

            if (radius < kMinOccluderRadius)
                continue;
            if (!frustum.IsSphereVisible(center, radius))
                continue;

            float dist = glm::max(glm::distance(center, cameraPos) - radius, 0.1f);
            candidates.push_back({ mc.Mesh, mc.SubmeshIndex, transform, radius / dist });
        }

        std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) { return a.Score > b.Score; });

        uint32_t triangleBudget = kMaxOccluderTriangles;
        for (const auto& c : candidates)
        {
            const auto& submesh = c.Mesh->GetSubmeshes()[c.SubmeshIndex];

            uint32_t baseIndex = submesh.BaseIndex;
            uint32_t indexCount = submesh.IndexCount;

            uint32_t triCount = indexCount / 3;
            if (triCount > triangleBudget)
                continue;
            triangleBudget -= triCount;

            const auto& vertices = c.Mesh->GetVertices();
            const auto& indices = c.Mesh->GetIndices();
            if (vertices.empty() || baseIndex + indexCount > indices.size())
                continue;

            m_SoftOcclusion.RasterizeOccluder(c.Transform,
                &vertices[0].Position, sizeof(Vertex),
                &indices[baseIndex], indexCount);
        }

        return m_SoftOcclusion.GetRasterizedTriangles() > 0;
    }

    void SceneRenderer::RenderScene(const Camera& camera, const glm::mat4& cameraTransform, bool showColliders, float deltaTime, const std::vector<Entity>& selectedEntities, const Ref<RenderTarget>& captureTarget)
    {
        bool isCapture = (captureTarget != nullptr);
        glm::mat4 viewProj = camera.GetProjection() * glm::inverse(cameraTransform);
        if (!isCapture)
        {
            if (m_IsFirstFrame)
            {
                m_PrevViewProjection = viewProj;
                m_IsFirstFrame = false;
            }
            else
            {
                m_PrevViewProjection = m_CurrentViewProjection;
            }
            m_CurrentViewProjection = viewProj;
        }

        Frustum frustum;
        frustum.Define(viewProj);

        LightEnvironment lightEnv;
        {
            auto view = m_Scene->m_Registry.view<DirectionalLightComponent, TransformComponent>();
            for (auto entity : view)
            {
                auto [light, transform] = view.get<DirectionalLightComponent, TransformComponent>(entity);
                glm::vec3 direction = glm::toMat3(glm::quat(transform.Rotation)) * glm::vec3(0, 0, -1);

                lightEnv.DirLight.Direction = direction;
                lightEnv.DirLight.Color = light.Color;
                lightEnv.DirLight.Intensity = light.Intensity;
                lightEnv.DirLight.CastsShadows = light.CastsShadows;
                lightEnv.DirLight.ShadowResolution = light.ShadowResolution;
            }
        }

        {
            auto view = m_Scene->m_Registry.view<PointLightComponent>();
            for (auto entity : view)
            {
                auto light = view.get<PointLightComponent>(entity);
                glm::mat4 worldTransform = m_Scene->GetWorldTransform({ entity, m_Scene.get() });
                glm::vec3 position = glm::vec3(worldTransform[3]);

                if (!frustum.IsSphereVisible(position, light.Radius))
                    continue;

                PointLight pl;
                pl.Position = position;
                pl.Color = light.Color;
                pl.Intensity = light.Intensity;
                pl.Radius = light.Radius;
                pl.Falloff = light.Falloff;
                pl.CastsShadows = light.CastsShadows;
                pl.ShadowResolution = light.ShadowResolution;
                pl.EntityID = (int)(uint32_t)entity;
                lightEnv.PointLights.push_back(pl);
            }
        }

        auto spotLightView = m_Scene->m_Registry.view<SpotLightComponent>();
        for (auto e : spotLightView)
        {
            auto& sl = spotLightView.get<SpotLightComponent>(e);
            if (sl.IsVideo && sl.CookieVideo)
                sl.CookieVideo->Update(deltaTime);
        }

        {
            auto view = m_Scene->m_Registry.view<SpotLightComponent>();
            for (auto entity : view)
            {
                auto light = view.get<SpotLightComponent>(entity);
                glm::mat4 worldTransform = m_Scene->GetWorldTransform({ entity, m_Scene.get() });
                glm::vec3 position = glm::vec3(worldTransform[3]);

                if (!frustum.IsSphereVisible(position, light.Radius))
                    continue;

                SpotLight sl;
                sl.Position = position;
                sl.Direction = glm::normalize(glm::vec3(worldTransform * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));

                glm::vec3 up = glm::normalize(glm::vec3(worldTransform * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));

                glm::mat4 lightView = glm::lookAt(sl.Position, sl.Position + sl.Direction, up);
                glm::mat4 lightProj = glm::perspective(glm::radians(light.OuterAngle * 2.0f), 1.0f, 0.1f, light.Radius);
                sl.LightSpaceMatrix = lightProj * lightView;

                sl.Color = light.Color;
                sl.Intensity = light.Intensity;
                sl.Radius = light.Radius;
                sl.Falloff = light.Falloff;
                sl.CutOff = glm::cos(glm::radians(light.InnerAngle));
                sl.OuterCutOff = glm::cos(glm::radians(light.OuterAngle));
                sl.CastsShadows = light.CastsShadows;
                sl.ShadowResolution = light.ShadowResolution;
                sl.EntityID = (int)(uint32_t)entity;

                sl.CookieTexture = light.IsVideo ?
                    (light.CookieVideo ? light.CookieVideo->GetTexture() : nullptr) : light.CookieTexture;

                sl.CookieSize = light.CookieSize;

                lightEnv.SpotLights.push_back(sl);
            }
        }

        glm::vec3 cameraPos = glm::vec3(cameraTransform[3]);
        glm::vec3 cameraForward = -glm::normalize(glm::vec3(cameraTransform[2]));
        glm::vec3 focusPoint = cameraPos + cameraForward * 5.0f;

        std::sort(lightEnv.PointLights.begin(), lightEnv.PointLights.end(), [&focusPoint](const PointLight& a, const PointLight& b)
            {
                return glm::distance(a.Position, focusPoint) < glm::distance(b.Position, focusPoint);
            });

        std::sort(lightEnv.SpotLights.begin(), lightEnv.SpotLights.end(), [&focusPoint](const SpotLight& a, const SpotLight& b)
            {
                glm::vec3 centerA = a.Position + (a.Direction * (a.Radius * 0.5f));
                glm::vec3 centerB = b.Position + (b.Direction * (b.Radius * 0.5f));
                return glm::distance(centerA, focusPoint) < glm::distance(centerB, focusPoint);
            });

        auto renderSys = Application::Get().GetSubsystem<Renderer>();
        auto jobSys = Application::Get().GetSubsystem<JobSystem>();

        lightEnv.EnvironmentIntensity = m_Scene->m_SkyboxIntensity;
        lightEnv.ShadowLightSize = GetSettings().LightSize;
        lightEnv.ShadowContactThreshold = GetSettings().ContactThreshold;
        lightEnv.ShadowContactSharpness = GetSettings().ContactSharpness;
        lightEnv.ShadowContactSharpeningBias = GetSettings().ContactSharpeningBias;
        lightEnv.SoftShadows = GetSettings().SoftShadows;

        if (!isCapture && m_HiZ)
        {
            RXN_GPU_SCOPE("Hi-Z Build");
            m_HiZ->Build(m_PrevResolvePass->GetDepthAttachmentRendererID(), glm::inverse(m_PrevViewProjection));
        }

        renderSys->BeginScene(camera, cameraTransform, lightEnv, m_Scene->m_Skybox, (isCapture ? captureTarget : m_GeoPass), m_Scene.get(), m_PrevViewProjection);

        RenderCommand::BindTextureID(15, m_PrevResolvePass->GetColorAttachmentRendererID());
        RenderCommand::BindTextureID(24, m_PrevResolvePass->GetDepthAttachmentRendererID());
        if (!isCapture && m_HiZ)
            RenderCommand::BindTextureID(30, m_HiZ->GetTextureID());

        std::vector<RenderCommandData> renderQueue;
        std::mutex queueMutex;

        s_HiZOccludedCount.store(0, std::memory_order_relaxed);
        bool occlusionReady = false;
        if (!isCapture)
            occlusionReady = RasterizeOccluders(viewProj, frustum, cameraPos);

        auto view = m_Scene->m_Registry.view<StaticMeshComponent, TransformComponent>();
        std::vector<entt::entity> entities(view.begin(), view.end());

        if (!entities.empty())
        {
            uint32_t threadCount = jobSys->GetThreadCount();
            uint32_t viewSize = (uint32_t)entities.size() / threadCount;
            if (viewSize == 0)
                viewSize = 1;

            jobSys->Dispatch(entities.size(), viewSize, [&](JobDispatchArgs args)
                {
                    entt::entity e = entities[args.JobIndex];
                    auto [mc, tc] = view.get<StaticMeshComponent, TransformComponent>(e);

                    if (!mc.Mesh)
                        return;

                    glm::mat4 transform = tc.WorldTransform;

                    Entity animatorParent;
                    if (e != entt::null && m_Scene->m_Registry.all_of<RelationshipComponent>(e))
                    {
                        auto& rc = m_Scene->m_Registry.get<RelationshipComponent>(e);
                        UUID parentHandle = rc.ParentHandle;
                        while (parentHandle != 0)
                        {
                            Entity parent = m_Scene->GetEntityByUUID(parentHandle);
                            if (parent)
                            {
                                if (parent.HasComponent<AnimatorComponent>())
                                {
                                    auto& animator = parent.GetComponent<AnimatorComponent>();
                                    if (animator.SkinnedVAO && animator.MeshAsset)
                                    {
                                        if (PathsMatch(mc.AssetPath, animator.MeshAssetPath))
                                        {
                                            animatorParent = parent;
                                        }
                                    }
                                    break;
                                }
                                if (parent.HasComponent<RelationshipComponent>())
                                {
                                    parentHandle = parent.GetComponent<RelationshipComponent>().ParentHandle;
                                }
                                else
                                {
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }

                    if (animatorParent)
                    {
                        transform = m_Scene->GetWorldTransform(animatorParent);
                    }

                    glm::vec3 worldPos = glm::vec3(transform[3]);

                    Ref<SkeletalMesh> skeletalMesh = animatorParent ? animatorParent.GetComponent<AnimatorComponent>().MeshAsset : nullptr;
                    const auto& submesh = (skeletalMesh && mc.SubmeshIndex < skeletalMesh->GetSubmeshes().size()) ? skeletalMesh->GetSubmeshes()[mc.SubmeshIndex] : mc.Mesh->GetSubmeshes()[mc.SubmeshIndex];
                    
                    AABB worldAABB;
                    if (animatorParent)
                    {
                        auto& animator = animatorParent.GetComponent<AnimatorComponent>();
                        if (!animator.CurrentPose.ModelTransforms.empty())
                        {
                            glm::vec3 minJoint(std::numeric_limits<float>::max());
                            glm::vec3 maxJoint(-std::numeric_limits<float>::max());
                            for (const auto& mat : animator.CurrentPose.ModelTransforms)
                            {
                                glm::vec3 pos = glm::vec3(mat[3]);
                                minJoint = glm::min(minJoint, pos);
                                maxJoint = glm::max(maxJoint, pos);
                            }

                            glm::vec3 bindExtents = (submesh.BoundingBox.Max - submesh.BoundingBox.Min) * 0.5f;
                            float bindRadius = glm::length(bindExtents);

                            AABB modelAABB;
                            modelAABB.Min = minJoint - glm::vec3(bindRadius);
                            modelAABB.Max = maxJoint + glm::vec3(bindRadius);

                            worldAABB = Math::CalculateWorldAABB(modelAABB, transform);
                        }
                        else
                        {
                            worldAABB = Math::CalculateWorldAABB(submesh.BoundingBox, transform);
                        }
                    }
                    else
                    {
                        worldAABB = Math::CalculateWorldAABB(submesh.BoundingBox, transform);
                    }

                    glm::vec3 center = (worldAABB.Min + worldAABB.Max) * 0.5f;
                    float radius = glm::distance(worldAABB.Min, worldAABB.Max) * 0.5f;

                    bool castsShadows = mc.CastsShadows;

                    bool isDynamic = animatorParent ? true : false;
                    if (!isDynamic && e != entt::null)
                    {
                        entt::entity current = e;
                        while (current != entt::null)
                        {
                            if (m_Scene->m_Registry.all_of<RigidbodyComponent>(current))
                            {
                                auto& rb = m_Scene->m_Registry.get<RigidbodyComponent>(current);
                                isDynamic = (rb.Type == RigidbodyComponent::BodyType::Dynamic || rb.Type == RigidbodyComponent::BodyType::Kinematic);
                                break;
                            }
                            if (m_Scene->m_Registry.all_of<CharacterControllerComponent>(current))
                            {
                                isDynamic = true;
                                break;
                            }
                            
                            if (m_Scene->m_Registry.all_of<RelationshipComponent>(current))
                            {
                                auto& rc = m_Scene->m_Registry.get<RelationshipComponent>(current);
                                if (rc.ParentHandle != 0)
                                {
                                    Entity parent = m_Scene->GetEntityByUUID(rc.ParentHandle);
                                    if (parent)
                                    {
                                        current = (entt::entity)parent;
                                        continue;
                                    }
                                }
                            }
                            break;
                        }
                    }

                    bool isVisible = frustum.IsSphereVisible(center, radius);

                    if (isVisible && occlusionReady)
                    {
                        glm::vec2 occlUVMin, occlUVMax;
                        float occlNearest;
                        if (ComputeOcclusionRect(worldAABB, viewProj, occlUVMin, occlUVMax, occlNearest))
                        {
                            if (m_SoftOcclusion.IsRectOccluded(occlUVMin, occlUVMax, occlNearest))
                            {
                                isVisible = false;
                                s_HiZOccludedCount.fetch_add(1, std::memory_order_relaxed);
                            }
                        }
                    }
                    float distanceToCam = glm::distance(worldPos, cameraPos);
                    float maxShadowDistance = 150.0f;

                    bool isVisibleToShadows = false;
                    if (castsShadows && distanceToCam <= maxShadowDistance + radius)
                    {
                        isVisibleToShadows = renderSys->IsSphereVisibleToShadows(center, radius);

                        if (e != entt::null && m_Scene->m_Registry.all_of<FoliageComponent>(e) && distanceToCam > 60.0f)
                        {
                            Entity foliageEntity = { e, m_Scene.get() };
                            renderSys->SubmitShadowImpostorQuad(foliageEntity);
                            isVisibleToShadows = false;
                        }
                    }

                    bool isVisibleToLocalLights = false;
                    if (castsShadows)
                    {
                        for (const auto& pl : lightEnv.PointLights)
                        {
                            if (pl.CastsShadows && glm::distance(center, pl.Position) <= pl.Radius + radius)
                            {
                                isVisibleToLocalLights = true;
                                break;
                            }
                        }
                        if (!isVisibleToLocalLights)
                        {
                            for (const auto& sl : lightEnv.SpotLights)
                            {
                                if (sl.CastsShadows && glm::distance(center, sl.Position) <= sl.Radius + radius)
                                {
                                    isVisibleToLocalLights = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (!isVisible && !isVisibleToShadows && !isVisibleToLocalLights)
                        return;

                    uint32_t lodIndex = 0;
                    if (!submesh.LODs.empty())
                    {
                        float distToCamLOD = glm::distance(center, cameraPos);
                        float screenPercentage = (distToCamLOD > 0.0001f) ? (radius / distToCamLOD) : 999.0f;

                        if (screenPercentage < 0.04f)
                            lodIndex = 2;
                        else if (screenPercentage < 0.12f)
                            lodIndex = 1;
                        else
                            lodIndex = 0;

                        if (lodIndex >= (uint32_t)submesh.LODs.size())
                            lodIndex = (uint32_t)submesh.LODs.size() - 1;
                    }

                    RenderCommandData cmd;
                    cmd.Mesh = mc.Mesh;
                    cmd.SubmeshIndex = mc.SubmeshIndex;
                    uint32_t matIndex = mc.Mesh->GetSubmeshes()[mc.SubmeshIndex].MaterialIndex;
                    cmd.Material = mc.MaterialTableOverride ? mc.MaterialTableOverride : mc.Mesh->GetMaterials()[matIndex];
                    cmd.Transform = transform;
                    cmd.EntityID = (int)(uint32_t)e;

                    cmd.BoundingCenter = center;
                    cmd.BoundingRadius = radius;
                    cmd.IsDynamic = isDynamic;

                    cmd.IsVisibleToCamera = isVisible;
                    cmd.IsVisibleToShadows = castsShadows && (isVisible || isVisibleToShadows || isVisibleToLocalLights);
                    cmd.LODIndex = lodIndex;

                    {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        renderQueue.push_back(cmd);
                    }
                });

            jobSys->Wait();
        }

        if (!isCapture)
            m_LastHiZOccluded = s_HiZOccludedCount.load(std::memory_order_relaxed);

        for (const auto& cmd : renderQueue)
        {
            if (cmd.IsVisibleToCamera)
                renderSys->Submit(cmd.Mesh, cmd.SubmeshIndex, cmd.Material, cmd.Transform, cmd.EntityID, cmd.LODIndex);

            if (cmd.IsVisibleToShadows)
                renderSys->SubmitShadowCaster(cmd.Mesh, cmd.SubmeshIndex, cmd.Transform, cmd.EntityID, cmd.BoundingCenter, cmd.BoundingRadius, cmd.IsDynamic, cmd.LODIndex);
        }

        if (m_Scene->m_Skybox)
            renderSys->DrawSkybox(m_Scene->m_Skybox, camera, cameraTransform);

        if (GetSettings().ShowColliders)
        {
            {
                auto view = m_Scene->m_Registry.view<TransformComponent, BoxColliderComponent>();
                for (auto entity : view)
                {
                    auto [tc, bc] = view.get<TransformComponent, BoxColliderComponent>(entity);

                    glm::mat4 worldTransform = m_Scene->GetWorldTransform({ entity, m_Scene.get() });
                    glm::vec3 worldTranslation, worldRotation, worldScale;
                    Math::DecomposeTransform(worldTransform, worldTranslation, worldRotation, worldScale);

                    glm::vec3 scale = worldScale * bc.HalfExtents * 2.0f;

                    glm::mat4 transform = glm::translate(glm::mat4(1.0f), worldTranslation)
                        * glm::toMat4(glm::quat(worldRotation))
                        * glm::translate(glm::mat4(1.0f), bc.Offset)
                        * glm::scale(glm::mat4(1.0f), scale);

                    renderSys->DrawWireBox(transform, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
                }
            }

            {
                auto view = m_Scene->m_Registry.view<TransformComponent, SphereColliderComponent>();
                for (auto entity : view)
                {
                    auto [tc, sc] = view.get<TransformComponent, SphereColliderComponent>(entity);

                    glm::mat4 worldTransform = m_Scene->GetWorldTransform({ entity, m_Scene.get() });
                    glm::vec3 worldTranslation, worldRotation, worldScale;
                    Math::DecomposeTransform(worldTransform, worldTranslation, worldRotation, worldScale);

                    float maxScale = glm::max(worldScale.x, glm::max(worldScale.y, worldScale.z));
                    glm::vec3 scale = glm::vec3(sc.Radius * maxScale);

                    glm::mat4 transform = glm::translate(glm::mat4(1.0f), worldTranslation)
                        * glm::toMat4(glm::quat(worldRotation))
                        * glm::translate(glm::mat4(1.0f), sc.Offset)
                        * glm::scale(glm::mat4(1.0f), scale);

                    renderSys->DrawWireSphere(transform, glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
                }
            }

            {
                auto view = m_Scene->m_Registry.view<TransformComponent, CapsuleColliderComponent>();
                for (auto entity : view)
                {
                    auto [tc, cc] = view.get<TransformComponent, CapsuleColliderComponent>(entity);

                    glm::mat4 worldTransform = m_Scene->GetWorldTransform({ entity, m_Scene.get() });
                    glm::vec3 worldTranslation, worldRotation, worldScale;
                    Math::DecomposeTransform(worldTransform, worldTranslation, worldRotation, worldScale);

                    float radiusScale = glm::max(worldScale.x, worldScale.z);
                    float radius = cc.Radius * radiusScale;
                    float height = cc.Height * worldScale.y;

                    glm::mat4 transform = glm::translate(glm::mat4(1.0f), worldTranslation)
                        * glm::toMat4(glm::quat(worldRotation))
                        * glm::translate(glm::mat4(1.0f), cc.Offset);

                    renderSys->DrawWireCapsule(transform, radius, height, glm::vec4(0.0f, 0.8f, 1.0f, 1.0f));
                }
            }
            {
                auto view = m_Scene->m_Registry.view<TransformComponent, MeshColliderComponent>();
                for (auto entityID : view)
                {
                    Entity entity = { entityID, m_Scene.get() };
                    auto [tc, mc] = view.get<TransformComponent, MeshColliderComponent>(entityID);

                    glm::mat4 worldTransform = m_Scene->GetWorldTransform(entity);

                    Ref<StaticMesh> collisionMesh = nullptr;

                    if (!mc.OverrideAssetPath.empty())
                    {
                        collisionMesh = Application::Get().GetSubsystem<AssetManager>()->GetMesh(mc.OverrideAssetPath);
                    }
                    else
                    {
                        std::function<Ref<StaticMesh>(Entity)> findMesh = [&](Entity e) -> Ref<StaticMesh>
                            {
                                if (e.HasComponent<StaticMeshComponent>())
                                    return e.GetComponent<StaticMeshComponent>().Mesh;

                                if (e.HasComponent<RelationshipComponent>()) 
                                {
                                    for (UUID childID : e.GetComponent<RelationshipComponent>().Children)
                                    {
                                        Entity child = m_Scene->GetEntityByUUID(childID);
                                        if (child)
                                        {
                                            Ref<StaticMesh> m = findMesh(child);

                                            if (m)
                                                return m;
                                        }
                                    }
                                }
                                return nullptr;
                            };
                        collisionMesh = findMesh(entity);
                    }

                    if (collisionMesh)
                    {
                        glm::vec4 color = mc.IsConvex ? glm::vec4(0.8f, 0.2f, 0.8f, 1.0f) : glm::vec4(0.2f, 0.8f, 0.8f, 1.0f);

                        if (mc.IsConvex)
                        {
                            bool hasHulls = false;
                            for (const auto& submesh : collisionMesh->GetSubmeshes())
                            {
                                if (!submesh.ConvexHulls.empty())
                                {
                                    hasHulls = true;
                                    for (const auto& hull : submesh.ConvexHulls)
                                    {
                                        for (size_t i = 0; i < hull.Indices.size(); i += 3)
                                        {
                                            glm::vec3 p0 = glm::vec3(worldTransform * submesh.LocalTransform * glm::vec4(hull.Vertices[hull.Indices[i]], 1.0f));
                                            glm::vec3 p1 = glm::vec3(worldTransform * submesh.LocalTransform * glm::vec4(hull.Vertices[hull.Indices[i + 1]], 1.0f));
                                            glm::vec3 p2 = glm::vec3(worldTransform * submesh.LocalTransform * glm::vec4(hull.Vertices[hull.Indices[i + 2]], 1.0f));

                                            renderSys->DrawLine(p0, p1, color);
                                            renderSys->DrawLine(p1, p2, color);
                                            renderSys->DrawLine(p2, p0, color);
                                        }
                                    }
                                }
                            }

                            if (!hasHulls)
                                renderSys->DrawWireBox(worldTransform, color);
                        }
                        else
                        {
                            for (const auto& submesh : collisionMesh->GetSubmeshes())
                            {
                                const auto& vertices = collisionMesh->GetVertices();
                                const auto& indices = collisionMesh->GetIndices();

                                for (size_t i = 0; i < submesh.IndexCount; i += 3)
                                {
                                    uint32_t idx0 = indices[submesh.BaseIndex + i];
                                    uint32_t idx1 = indices[submesh.BaseIndex + i + 1];
                                    uint32_t idx2 = indices[submesh.BaseIndex + i + 2];

                                    glm::vec3 p0 = glm::vec3(worldTransform * submesh.LocalTransform * glm::vec4(vertices[idx0].Position, 1.0f));
                                    glm::vec3 p1 = glm::vec3(worldTransform * submesh.LocalTransform * glm::vec4(vertices[idx1].Position, 1.0f));
                                    glm::vec3 p2 = glm::vec3(worldTransform * submesh.LocalTransform * glm::vec4(vertices[idx2].Position, 1.0f));

                                    renderSys->DrawLine(p0, p1, color);
                                    renderSys->DrawLine(p1, p2, color);
                                    renderSys->DrawLine(p2, p0, color);
                                }
                            }
                        }
                    }
                }
            }
        }

        for (Entity selected : selectedEntities)
        {
            if (!selected)
                continue;

            glm::mat4 transform = m_Scene->GetWorldTransform(selected);

            glm::vec3 wTranslation, wRotation, wScale;
            Math::DecomposeTransform(transform, wTranslation, wRotation, wScale);
            glm::mat4 unscaledTransform = glm::translate(glm::mat4(1.0f), wTranslation) * glm::toMat4(glm::quat(wRotation));

            if (selected.HasComponent<CameraComponent>())
            {
                auto& cc = selected.GetComponent<CameraComponent>();
                renderSys->DrawFrustum(transform, cc.Camera.GetProjection(), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
            }

            if (selected.HasComponent<DirectionalLightComponent>())
            {
                auto& dlc = selected.GetComponent<DirectionalLightComponent>();
                glm::mat4 transform = m_Scene->GetWorldTransform(selected);
                renderSys->DrawArrow(transform, glm::vec4(1.0f, 0.9f, 0.1f, 1.0f));
            }

            if (selected.HasComponent<PointLightComponent>())
            {
                auto& plc = selected.GetComponent<PointLightComponent>();
                glm::mat4 scaleTransform = glm::scale(unscaledTransform, glm::vec3(plc.Radius));
                renderSys->DrawWireSphere(scaleTransform, glm::vec4(1.0f, 0.8f, 0.1f, 1.0f));
            }

            if (selected.HasComponent<SpotLightComponent>())
            {
                auto& slc = selected.GetComponent<SpotLightComponent>();
                renderSys->DrawWireCone(unscaledTransform, slc.Radius, slc.InnerAngle, glm::vec4(1.0f, 0.8f, 0.1f, 0.3f));
                renderSys->DrawWireCone(unscaledTransform, slc.Radius, slc.OuterAngle, glm::vec4(1.0f, 0.8f, 0.1f, 1.0f));
            }

            if (selected.HasComponent<AudioSourceComponent>())
            {
                auto& ac = selected.GetComponent<AudioSourceComponent>();
                glm::mat4 minTransform = glm::scale(unscaledTransform, glm::vec3(ac.MinDistance));
                glm::mat4 maxTransform = glm::scale(unscaledTransform, glm::vec3(ac.MaxDistance));

                renderSys->DrawWireSphere(minTransform, glm::vec4(0.2f, 0.8f, 0.2f, 1.0f));
                renderSys->DrawWireSphere(maxTransform, glm::vec4(0.8f, 0.2f, 0.2f, 1.0f));
            }

            if (selected.HasComponent<AudioReverbZoneComponent>())
            {
                auto& azc = selected.GetComponent<AudioReverbZoneComponent>();
                if (azc.IsBox)
                {
                    glm::mat4 scaleTransform = glm::scale(unscaledTransform, azc.BoxDimensions);
                    renderSys->DrawWireBox(scaleTransform, glm::vec4(0.0f, 0.6f, 1.0f, 1.0f));
                }
                else
                {
                    glm::mat4 minTransform = glm::scale(unscaledTransform, glm::vec3(azc.MinDistance));
                    glm::mat4 maxTransform = glm::scale(unscaledTransform, glm::vec3(azc.MaxDistance));
                    renderSys->DrawWireSphere(minTransform, glm::vec4(0.0f, 0.6f, 1.0f, 0.5f));
                    renderSys->DrawWireSphere(maxTransform, glm::vec4(0.0f, 0.6f, 1.0f, 1.0f));
                }
            }

            if (selected.HasComponent<AudioPortalComponent>())
            {
                auto& apc = selected.GetComponent<AudioPortalComponent>();
                glm::mat4 scaleTransform = glm::scale(unscaledTransform, glm::vec3(apc.Dimensions.x, apc.Dimensions.y, 0.05f));
                renderSys->DrawWireBox(scaleTransform, glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
            }
        }

        renderSys->EndScene();

    }

    void SceneRenderer::RenderRuntime(uint32_t targetWidth, uint32_t targetHeight)
    {
        RXN_PROFILE_SCOPE();

        GPUProfiler::Get().BeginFrame();

        std::swap(m_ResolvePass, m_PrevResolvePass);
        {
            RXN_GPU_SCOPE("PrevFrame Mipmaps");
            m_PrevResolvePass->GenerateMipmaps(0);
        }

        Entity cameraEntity = m_Scene->GetPrimaryCameraEntity();

        if (!cameraEntity)
        {
            GPUProfiler::Get().EndFrame();
            return;
        }

        Camera& camera = cameraEntity.GetComponent<CameraComponent>().Camera;
        glm::mat4 cameraTransform = m_Scene->GetWorldTransform(cameraEntity);
        Ref<RenderTarget>& renderTarget = m_GeoPass;
        bool showColliders = GetSettings().ShowColliders;

        if (targetWidth > 0 && targetHeight > 0 && (m_ViewportWidth != targetWidth || m_ViewportHeight != targetHeight))
            SetViewportSize(targetWidth, targetHeight);

        if (m_OutlineMaskPass)
        {
            m_OutlineMaskPass->Bind();
            RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 0.0f });
            RenderCommand::Clear();
            m_OutlineMaskPass->Unbind();
        }

        m_GeoPass->Bind();
        RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
        RenderCommand::SetDepthTest(true);
        RenderCommand::Clear();
        m_GeoPass->ClearColorAttachmentFloat(1, 0.0f, 0.0f, 0.0f, 0.0f);
        m_GeoPass->ClearColorAttachmentFloat(2, 0.5f, 0.5f, 1.0f, 0.0f);

        RenderScene(camera, cameraTransform, GetSettings().ShowColliders);

        renderTarget->Bind();
        RenderCommand::SetBlend(true);
        RenderUI(camera, cameraTransform, true);   // World Space
        renderTarget->Unbind();

        ResolveMSAA();
        RenderAO(camera, cameraTransform);
        RenderBloom();
        RenderSSRDenoise();
        RenderPostProcess();

        m_FinalPass->Bind();
        RenderCommand::SetBlend(true);
        RenderUI(camera, cameraTransform, false);  // Screen Space
        m_FinalPass->Unbind();

        GPUProfiler::Get().EndFrame();
    }

    void SceneRenderer::RenderUI(const Camera& camera, const glm::mat4& cameraTransform, bool worldSpace, const std::vector<Entity>& selectedEntities)
    {
        GBufferMaskGuard uiMaskGuard;

        RenderCommand::SetCullFace(RendererAPI::CullFace::None);
        RenderCommand::SetDepthTest(worldSpace);

        glm::mat4 viewProj;
        if (worldSpace)
            viewProj = camera.GetProjection() * glm::inverse(cameraTransform);
        else
            viewProj = glm::ortho(0.0f, (float)m_ViewportWidth, 0.0f, (float)m_ViewportHeight, -1.0f, 1.0f);

        bool useCanvasFilter = !worldSpace && !selectedEntities.empty();
        std::unordered_set<UUID> allowedCanvases;

        if (useCanvasFilter)
        {
            for (Entity selected : selectedEntities)
            {
                if (!selected)
                    continue;

                Entity curr = selected;
                while (curr)
                {
                    if (curr.HasComponent<UICanvasComponent>())
                    {
                        allowedCanvases.insert(curr.GetUUID());
                        break;
                    }

                    if (curr.HasComponent<RelationshipComponent>())
                    {
                        UUID pid = curr.GetComponent<RelationshipComponent>().ParentHandle;
                        curr = pid != 0 ? m_Scene->GetEntityByUUID(pid) : Entity{};
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }

        struct UIRenderItem
        {
            Entity entity;
            int zIndex;
            glm::mat4 canvasTransform;
        };
        std::vector<UIRenderItem> renderItems;

        std::function<void(Entity, const glm::mat4&)> collectChildren = [&](Entity parent, const glm::mat4& canvasT)
            {
                if (!parent.HasComponent<RelationshipComponent>())
                    return;

                auto& rc = parent.GetComponent<RelationshipComponent>();
                for (UUID childID : rc.Children)
                {
                    Entity child = m_Scene->GetEntityByUUID(childID);

                    if (!child)
                        continue;

                    if (child.HasComponent<UITransformComponent>())
                    {
                        bool hasVisual = child.HasComponent<UIImageComponent>() || child.HasComponent<UITextComponent>();
                        if (hasVisual)
                            renderItems.push_back({ child, child.GetComponent<UITransformComponent>().ZIndex, canvasT });
                    }
                    collectChildren(child, canvasT);
                }
            };

        auto canvasView = m_Scene->m_Registry.view<UICanvasComponent>();
        for (auto canvasEntity : canvasView)
        {
            auto& canvas = canvasView.get<UICanvasComponent>(canvasEntity);
            if (!canvas.Active)
                continue;

            bool isWorldSpace = canvas.RenderMode == CanvasRenderMode::WorldSpace;
            if (isWorldSpace != worldSpace)
                continue;

            Entity e = { canvasEntity, m_Scene.get() };

            if (useCanvasFilter && allowedCanvases.find(e.GetUUID()) == allowedCanvases.end())
                continue;   

            glm::mat4 canvasTransform = glm::mat4(1.0f);
            if (isWorldSpace)
            {
                if (e.HasComponent<TransformComponent>())
                    canvasTransform = m_Scene->GetWorldTransform(e);
            }

            if (e.HasComponent<UITransformComponent>())
            {
                bool hasVisual = e.HasComponent<UIImageComponent>() || e.HasComponent<UITextComponent>();
                if (hasVisual)
                    renderItems.push_back({ e, e.GetComponent<UITransformComponent>().ZIndex, canvasTransform });
            }

            collectChildren(e, canvasTransform);
        }

        if (renderItems.empty())
        {
            RenderCommand::SetCullFace(RendererAPI::CullFace::Back);
            RenderCommand::SetDepthTest(true);
            return;
        }

        std::sort(renderItems.begin(), renderItems.end(), [](const UIRenderItem& a, const UIRenderItem& b)
            {
                return a.zIndex < b.zIndex;
            });

        Renderer2D::BeginScene(viewProj);

        for (const auto& item : renderItems)
        {
            Entity entity = item.entity;
            auto& transform = entity.GetComponent<UITransformComponent>();

            glm::mat4 renderTransform = transform.ComputedTransform;

            if (entity.HasComponent<UIImageComponent>())
            {
                auto& image = entity.GetComponent<UIImageComponent>();
                if (image.Texture)
                    Renderer2D::DrawQuad(renderTransform, image.Texture, image.TintColor);
                else
                    Renderer2D::DrawQuad(renderTransform, image.TintColor);
            }

            if (entity.HasComponent<UITextComponent>())
            {
                auto& text = entity.GetComponent<UITextComponent>();
                if (!text.Text.empty() && text.FontAsset)
                {
                    Renderer2D::TextParams params;
                    params.Color = text.Color;
                    params.Kerning = text.Kerning;
                    params.LineSpacing = text.LineSpacing;

                    size_t hash = std::hash<std::string>()(text.Text);
                    auto hashCombine = [](size_t& seed, size_t hashVal) { seed ^= hashVal + 0x9e3779b9 + (seed << 6) + (seed >> 2); };
                    hashCombine(hash, std::hash<float>()(text.FontSize));
                    hashCombine(hash, std::hash<float>()(text.LineSpacing));
                    hashCombine(hash, std::hash<float>()(text.Kerning));
                    hashCombine(hash, std::hash<void*>()(text.FontAsset.get()));

                    if (hash != text.TextHash)
                    {
                        Renderer2D::BuildTextGeometry(text.Text, text.FontAsset, params, text.CachedGeometry);
                        text.TextHash = hash;
                    }

                    glm::vec3 textLocalPos = glm::vec3(transform.ComputedBoundsMin.x, transform.ComputedBoundsMax.y - text.FontSize, 0.0f);
                    glm::mat4 textLocalTransform = glm::translate(glm::mat4(1.0f), textLocalPos)
                        * glm::scale(glm::mat4(1.0f), glm::vec3(text.FontSize, text.FontSize, 1.0f));

                    glm::mat4 finalTextTransform = item.canvasTransform * textLocalTransform;

                    Renderer2D::DrawTextCached(text.CachedGeometry, finalTextTransform, text.FontAsset, params);
                }
            }
        }

        Renderer2D::EndScene();
        RenderCommand::SetCullFace(RendererAPI::CullFace::Back);
    
    }

    void SceneRenderer::RenderUIPicking(const EditorCamera& camera, const std::vector<Entity>& selectedEntities)
    {
        bool showScreenSpaceUI = false;
        for (Entity selected : selectedEntities)
        {
            if (selected && (selected.HasComponent<UITransformComponent>() || selected.HasComponent<UICanvasComponent>()))
            {
                showScreenSpaceUI = true;
                break;
            }
        }

        auto renderUIPickingPass = [&](bool worldSpace)
        {
            if (!worldSpace && !showScreenSpaceUI)
                return;

            RenderCommand::SetCullFace(RendererAPI::CullFace::None);
            RenderCommand::SetDepthTest(worldSpace);

            glm::mat4 viewProj;
            if (worldSpace)
                viewProj = camera.GetViewProjection();
            else
                viewProj = glm::ortho(0.0f, (float)m_ViewportWidth, 0.0f, (float)m_ViewportHeight, -1.0f, 1.0f);

            bool useCanvasFilter = !worldSpace && !selectedEntities.empty();
            std::unordered_set<UUID> allowedCanvases;

            if (useCanvasFilter)
            {
                for (Entity selected : selectedEntities)
                {
                    if (!selected)
                        continue;

                    Entity curr = selected;
                    while (curr)
                    {
                        if (curr.HasComponent<UICanvasComponent>())
                        {
                            allowedCanvases.insert(curr.GetUUID());
                            break;
                        }
                        if (curr.HasComponent<RelationshipComponent>())
                        {
                            UUID pid = curr.GetComponent<RelationshipComponent>().ParentHandle;
                            curr = pid != 0 ? m_Scene->GetEntityByUUID(pid) : Entity{};
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }

            struct UIRenderItem
            {
                Entity entity;
                int zIndex;
                glm::mat4 canvasTransform;
            };
            std::vector<UIRenderItem> renderItems;

            std::function<void(Entity, const glm::mat4&)> collectChildren = [&](Entity parent, const glm::mat4& canvasT)
            {
                if (!parent.HasComponent<RelationshipComponent>()) 
                    return;

                auto& rc = parent.GetComponent<RelationshipComponent>();
                for (UUID childID : rc.Children)
                {
                    Entity child = m_Scene->GetEntityByUUID(childID);
                    if (!child)
                        continue;

                    if (child.HasComponent<UITransformComponent>())
                    {
                        bool hasVisual = child.HasComponent<UIImageComponent>() || child.HasComponent<UITextComponent>();
                        if (hasVisual)
                            renderItems.push_back({ child, child.GetComponent<UITransformComponent>().ZIndex, canvasT });
                    }
                    collectChildren(child, canvasT);
                }
            };

            auto canvasView = m_Scene->m_Registry.view<UICanvasComponent>();
            for (auto canvasEntity : canvasView)
            {
                auto& canvas = canvasView.get<UICanvasComponent>(canvasEntity);
                if (!canvas.Active)
                    continue;

                bool isWorldSpace = canvas.RenderMode == CanvasRenderMode::WorldSpace;
                if (isWorldSpace != worldSpace)
                    continue;

                Entity e = { canvasEntity, m_Scene.get() };

                if (useCanvasFilter && allowedCanvases.find(e.GetUUID()) == allowedCanvases.end())
                    continue;

                glm::mat4 canvasTransform = glm::mat4(1.0f);
                if (isWorldSpace)
                {
                    if (e.HasComponent<TransformComponent>())
                        canvasTransform = m_Scene->GetWorldTransform(e);
                }

                if (e.HasComponent<UITransformComponent>())
                {
                    bool hasVisual = e.HasComponent<UIImageComponent>() || e.HasComponent<UITextComponent>();
                    if (hasVisual)
                        renderItems.push_back({ e, e.GetComponent<UITransformComponent>().ZIndex, canvasTransform });
                }

                collectChildren(e, canvasTransform);
            }

            if (renderItems.empty())
                return;

            std::sort(renderItems.begin(), renderItems.end(), [](const UIRenderItem& a, const UIRenderItem& b)
            {
                return a.zIndex < b.zIndex;
            });

            m_UIPickingShader->Bind();
            m_UIPickingShader->SetMat4("u_ViewProjection", viewProj);

            for (const auto& item : renderItems)
            {
                Entity entity = item.entity;
                auto& transform = entity.GetComponent<UITransformComponent>();

                glm::mat4 renderTransform = transform.ComputedTransform;

                m_UIPickingShader->SetMat4("u_Model", renderTransform);
                m_UIPickingShader->SetInt("u_EntityID", (int)(uint32_t)entity);

                m_UIPickingQuadVAO->Bind();
                RenderCommand::DrawIndexed(m_UIPickingQuadVAO, 6);
            }
        };

        renderUIPickingPass(true);  // World Space
        renderUIPickingPass(false); // Screen Space
        RenderCommand::SetCullFace(RendererAPI::CullFace::Back);
        RenderCommand::SetDepthTest(true);
    
    }

    void SceneRenderer::RenderGridOverlay(EditorCamera& camera)
    {
        RXN_GPU_SCOPE("Grid Overlay");
        if (!m_GridShader || !m_FinalPass || !m_ResolvePass) return;

        const auto& spec = m_FinalPass->GetSpecification();
        m_FinalPass->Bind();
        RenderCommand::SetViewport(0, 0, spec.Width, spec.Height);
        RenderCommand::SetDepthTest(false);
        RenderCommand::SetBlend(true);
        RenderCommand::SetBlendFunc(RendererAPI::BlendFactor::SrcAlpha,
            RendererAPI::BlendFactor::OneMinusSrcAlpha);
        RenderCommand::SetCullFace(RendererAPI::CullFace::None);

        m_GridShader->Bind();
        m_GridShader->SetMat4("u_ViewProjection", camera.GetViewProjection());
        m_GridShader->SetFloat3("u_CameraPos", camera.GetPosition());
        m_GridShader->SetInt("u_SceneDepth", 0);
        m_GridShader->SetFloat2("u_ScreenSize", glm::vec2((float)spec.Width, (float)spec.Height));
        RenderCommand::BindTextureID(0, m_ResolvePass->GetDepthAttachmentRendererID());

        m_GridQuadVAO->Bind();
        RenderCommand::DrawIndexed(m_GridQuadVAO);

        RenderCommand::SetCullFace(RendererAPI::CullFace::Back);
        RenderCommand::SetDepthTest(true);
        m_FinalPass->Unbind();
    }
}