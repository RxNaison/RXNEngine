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
#include <glm/gtc/matrix_transform.hpp>

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

    struct ViewFrustum
    {
        glm::vec4 Planes[6];

        void Extract(const glm::mat4& viewProj)
        {
            for (int i = 0; i < 4; ++i)
                Planes[0][i] = viewProj[i][3] + viewProj[i][0]; // Left

            for (int i = 0; i < 4; ++i)
                Planes[1][i] = viewProj[i][3] - viewProj[i][0]; // Right

            for (int i = 0; i < 4; ++i)
                Planes[2][i] = viewProj[i][3] + viewProj[i][1]; // Bottom

            for (int i = 0; i < 4; ++i)
                Planes[3][i] = viewProj[i][3] - viewProj[i][1]; // Top

            for (int i = 0; i < 4; ++i)
                Planes[4][i] = viewProj[i][3] + viewProj[i][2]; // Near

            for (int i = 0; i < 4; ++i)
                Planes[5][i] = viewProj[i][3] - viewProj[i][2]; // Far

            for (int i = 0; i < 6; ++i) {
                float length = glm::length(glm::vec3(Planes[i]));
                Planes[i] /= length;
            }
        }

        bool IsSphereVisible(const glm::vec3& center, float radius) const
        {
            for (int i = 0; i < 6; ++i)
            {
                if (glm::dot(glm::vec3(Planes[i]), center) + Planes[i].w < -radius)
                    return false;
            }
            return true;
        }
    };

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
        geoSpec.Attachments = { RenderTargetTextureFormat::RGBA16F, RenderTargetTextureFormat::Depth };
        geoSpec.Width = width;
        geoSpec.Height = height;
        m_GeoPass = RenderTarget::Create(geoSpec);

        RenderTargetSpecification finalSpec;
        finalSpec.Attachments = { RenderTargetTextureFormat::RGBA8 };
        finalSpec.Width = width;
        finalSpec.Height = height;
        m_FinalPass = RenderTarget::Create(finalSpec);

        RenderTargetSpecification pickSpec;
        pickSpec.Attachments = { RenderTargetTextureFormat::RED_INTEGER, RenderTargetTextureFormat::Depth };
        pickSpec.Width = width;
        pickSpec.Height = height;
        m_PickingPass = RenderTarget::Create(pickSpec);

        RenderTargetSpecification maskSpec;
        maskSpec.Attachments = { RenderTargetTextureFormat::RGBA8 };
        maskSpec.Width = width;
        maskSpec.Height = height;
        m_OutlineMaskPass = RenderTarget::Create(maskSpec);


        m_PostProcessShader = Shader::Create("res/shaders/postprocess/screen.glsl");
        m_BloomDownsampleShader = Shader::Create("res/shaders/postprocess/bloom_downsample.glsl");
        m_BloomUpsampleShader = Shader::Create("res/shaders/postprocess/bloom_upsample.glsl");
        m_PickingShader = Shader::Create("res/shaders/editor_picking.glsl");
        m_UIPickingShader = Shader::Create("res/shaders/editor_ui_picking.glsl");
        m_GridShader = Shader::Create("res/shaders/grid.glsl");
        m_OutlineMaskShader = Shader::Create("res/shaders/outline_mask.glsl");

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
            m_FinalPass->Resize(width, height);
            m_PickingPass->Resize(width, height);
            m_OutlineMaskPass->Resize(width, height);
            m_Scene->OnViewportResize(width, height);

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

    void SceneRenderer::RenderEditor(uint32_t targetWidth, uint32_t targetHeight, float deltaTime, EditorCamera& camera, const std::vector<Entity>& selectedEntities)
    {
        RXN_PROFILE_SCOPE();

        if (targetWidth > 0 && targetHeight > 0 && (m_ViewportWidth != targetWidth || m_ViewportHeight != targetHeight))
        {
            SetViewportSize(targetWidth, targetHeight);
            camera.SetViewportSize((float)targetWidth, (float)targetHeight);
        }

        m_OutlineMaskPass->Bind();
        RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        RenderCommand::Clear();

        bool showScreenSpaceUI = false;

        if (!selectedEntities.empty())
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
                            Application::Get().GetSubsystem<Renderer>()->DrawEntityOutline(mc.Mesh, mc.SubmeshIndex, m_Scene->GetWorldTransform(entity), m_OutlineMaskShader);
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
                    {
                        showScreenSpaceUI = true;
                    }
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
        m_OutlineMaskPass->Unbind();

        m_GeoPass->Bind();
        RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
        RenderCommand::Clear();

        m_Scene->UpdateWorldTransforms();
        m_Scene->GetSubsystem<UISystem>()->Update(deltaTime);

        glm::mat4 cameraTransform = glm::inverse(camera.GetViewMatrix());
        RenderScene(camera, cameraTransform, m_Settings.ShowColliders, deltaTime, selectedEntities);

        m_GeoPass->Bind();
        RenderCommand::SetBlend(true);
        glm::mat4 editorCameraTransform = glm::inverse(camera.GetViewMatrix());
        RenderUI(camera, editorCameraTransform, true);   // World Space

        if (showScreenSpaceUI)
            RenderUI(camera, editorCameraTransform, false, selectedEntities);  // Screen Space

        RenderCommand::SetBlend(true);
        RenderCommand::SetDepthTest(true);
        RenderCommand::SetCullFace(RendererAPI::CullFace::None);

        m_GridShader->Bind();
        m_GridShader->SetMat4("u_ViewProjection", camera.GetViewProjection());
        m_GridShader->SetFloat3("u_CameraPos", camera.GetPosition());
        m_GridQuadVAO->Bind();
        RenderCommand::DrawIndexed(m_GridQuadVAO);

        RenderCommand::SetCullFace(RendererAPI::CullFace::Back);
        m_GeoPass->Unbind();

        RenderBloom();
        RenderPostProcess();
    }



    void SceneRenderer::RenderToTarget(uint32_t width, uint32_t height, Camera& camera, const glm::mat4& cameraTransform)
    {
        RXN_PROFILE_SCOPE();

        SetViewportSize(width, height);

        m_OutlineMaskPass->Bind();
        RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        RenderCommand::Clear();
        m_OutlineMaskPass->Unbind();

        m_GeoPass->Bind();
        RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
        RenderCommand::SetDepthTest(true);
        RenderCommand::Clear();

        RenderScene(camera, cameraTransform, false);

        m_GeoPass->Unbind();

        RenderBloom();
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

    void SceneRenderer::RenderPostProcess()
    {
        RXN_PROFILE_SCOPE();

        m_FinalPass->Bind();

        RenderCommand::SetDepthTest(false);
        RenderCommand::Clear();

        m_PostProcessShader->Bind();
        m_PostProcessShader->SetInt("u_ScreenTexture", 0);
        m_PostProcessShader->SetInt("u_BloomTexture", 1);
        m_PostProcessShader->SetInt("u_OutlineTexture", 2);

        const auto& spec = m_FinalPass->GetSpecification();
        m_PostProcessShader->SetFloat2("u_TexelSize", glm::vec2(1.0f / spec.Width, 1.0f / spec.Height));

        m_PostProcessShader->SetFloat("u_Exposure", m_Settings.Exposure);
        m_PostProcessShader->SetFloat("u_Gamma", m_Settings.Gamma);
        m_PostProcessShader->SetFloat("u_BloomIntensity", m_Settings.BloomIntensity);

        RenderCommand::BindTextureID(0, m_GeoPass->GetColorAttachmentRendererID());

        if (!m_BloomMips.empty())
            RenderCommand::BindTextureID(1, m_BloomMips[0].Target->GetColorAttachmentRendererID());

        RenderCommand::BindTextureID(2, m_OutlineMaskPass->GetColorAttachmentRendererID());

        m_ScreenQuadVAO->Bind();
        RenderCommand::DrawIndexed(m_ScreenQuadVAO);

        RenderCommand::SetDepthTest(true);

        m_FinalPass->Unbind();
    }

    void SceneRenderer::RenderBloom()
    {
        RXN_PROFILE_SCOPE();

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
        m_BloomDownsampleShader->SetFloat4("u_Threshold", filter);

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


    void SceneRenderer::RenderScene(const Camera& camera, const glm::mat4& cameraTransform, bool showColliders, float deltaTime, const std::vector<Entity>& selectedEntities)
    {
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
            }
        }
        {
            auto view = m_Scene->m_Registry.view<PointLightComponent>();
            for (auto entity : view)
            {
                auto light = view.get<PointLightComponent>(entity);
                glm::mat4 worldTransform = m_Scene->GetWorldTransform({ entity, m_Scene.get() });

                PointLight pl;
                pl.Position = glm::vec3(worldTransform[3]);
                pl.Color = light.Color;
                pl.Intensity = light.Intensity;
                pl.Radius = light.Radius;
                pl.Falloff = light.Falloff;
                pl.CastsShadows = light.CastsShadows;
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

                SpotLight sl;
                sl.Position = glm::vec3(worldTransform[3]);
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
        renderSys->BeginScene(camera, cameraTransform, lightEnv, m_Scene->m_Skybox, m_GeoPass);

        glm::mat4 viewProj = camera.GetProjection() * glm::inverse(cameraTransform);
        ViewFrustum frustum;
        frustum.Extract(viewProj);

        std::vector<RenderCommandData> renderQueue;
        std::mutex queueMutex;

        auto view = m_Scene->m_Registry.view<StaticMeshComponent, TransformComponent>();
        std::vector<entt::entity> entities(view.begin(), view.end());

        if (!entities.empty())
        {
            uint32_t threadCount = jobSys->GetThreadCount();
            uint32_t viewSize = (uint32_t)entities.size() / threadCount;
            if (viewSize == 0) viewSize = 1;

            jobSys->Dispatch(entities.size(), viewSize, [&](JobDispatchArgs args)
                {
                    entt::entity e = entities[args.JobIndex];
                    auto [mc, tc] = view.get<StaticMeshComponent, TransformComponent>(e);

                    if (!mc.Mesh)
                        return;

                    glm::mat4 transform = tc.WorldTransform;
                    glm::vec3 worldPos = glm::vec3(transform[3]);

                    const auto& submesh = mc.Mesh->GetSubmeshes()[mc.SubmeshIndex];
                    AABB worldAABB = Math::CalculateWorldAABB(submesh.BoundingBox, transform);

                    glm::vec3 center = (worldAABB.Min + worldAABB.Max) * 0.5f;
                    float radius = glm::distance(worldAABB.Min, worldAABB.Max) * 0.5f;

                    bool isVisible = frustum.IsSphereVisible(center, radius);
                    bool isVisibleToShadows = renderSys->IsSphereVisibleToShadows(center, radius);

                    bool isVisibleToLocalLights = false;
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

                    if (!isVisible && !isVisibleToShadows && !isVisibleToLocalLights)
                        return;

                    RenderCommandData cmd;
                    cmd.Mesh = mc.Mesh;
                    cmd.SubmeshIndex = mc.SubmeshIndex;
                    uint32_t matIndex = mc.Mesh->GetSubmeshes()[mc.SubmeshIndex].MaterialIndex;
                    cmd.Material = mc.MaterialTableOverride ? mc.MaterialTableOverride : mc.Mesh->GetMaterials()[matIndex];
                    cmd.Transform = transform;
                    cmd.EntityID = (int)(uint32_t)e;

                    cmd.BoundingCenter = center;
                    cmd.BoundingRadius = radius;

                    cmd.IsVisibleToCamera = isVisible;
                    cmd.IsVisibleToShadows = isVisible || isVisibleToShadows || isVisibleToLocalLights;

                    {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        renderQueue.push_back(cmd);
                    }
                });

            jobSys->Wait();
        }

        for (const auto& cmd : renderQueue)
        {
            if (cmd.IsVisibleToCamera)
                renderSys->Submit(cmd.Mesh, cmd.SubmeshIndex, cmd.Material, cmd.Transform, cmd.EntityID);

            if (cmd.IsVisibleToShadows)
                renderSys->SubmitShadowCaster(cmd.Mesh, cmd.SubmeshIndex, cmd.Transform, cmd.EntityID, cmd.BoundingCenter, cmd.BoundingRadius);
        }

        if (m_Scene->m_Skybox)
            renderSys->DrawSkybox(m_Scene->m_Skybox, camera, cameraTransform);

        if (m_Settings.ShowColliders)
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
                            // This can be heavy if the mesh is 100k+ polys
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
        }

        renderSys->EndScene();

    }

    void SceneRenderer::RenderRuntime(uint32_t targetWidth, uint32_t targetHeight)
    {

        RXN_PROFILE_SCOPE();

        Entity cameraEntity = m_Scene->GetPrimaryCameraEntity();
        if (!cameraEntity) return;
        Camera& camera = cameraEntity.GetComponent<CameraComponent>().Camera;
        glm::mat4 cameraTransform = m_Scene->GetWorldTransform(cameraEntity);
        Ref<RenderTarget>& renderTarget = m_GeoPass;
        bool showColliders = m_Settings.ShowColliders;

        if (targetWidth > 0 && targetHeight > 0 && (m_ViewportWidth != targetWidth || m_ViewportHeight != targetHeight))
            SetViewportSize(targetWidth, targetHeight);

        m_OutlineMaskPass->Bind();
        RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        RenderCommand::Clear();
        m_OutlineMaskPass->Unbind();

        m_GeoPass->Bind();
        RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
        RenderCommand::SetDepthTest(true);
        RenderCommand::Clear();

        RenderScene(camera, cameraTransform, m_Settings.ShowColliders);

        renderTarget->Bind();
        RenderCommand::SetBlend(true);
        RenderUI(camera, cameraTransform, true);   // World Space
        RenderUI(camera, cameraTransform, false);  // Screen Space
        renderTarget->Unbind();

        RenderBloom();
        RenderPostProcess();
    
    }

    void SceneRenderer::RenderUI(const Camera& camera, const glm::mat4& cameraTransform, bool worldSpace, const std::vector<Entity>& selectedEntities)
    {
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
                    else break;
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
                        else break;
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
}