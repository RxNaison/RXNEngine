#include "rxnpch.h"
#include "RXNEngine/Renderer/GPUProfiler.h"
#include "Renderer.h"
#include "RXNEngine/Math/Math.h"
#include "RXNEngine/Math/Frustum.h"
#include "RXNEngine/Renderer/GraphicsAPI/UniformBuffer.h"
#include "RXNEngine/Renderer/GraphicsAPI/Texture.h"
#include "RXNEngine/Renderer/LightCuller.h"
#include "RenderCommand.h"
#include "ShadowMap.h"
#include "Renderer2D.h"
#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Scene/Components.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>

namespace RXNEngine {

    namespace {
        bool PathsMatch(const std::string& path1, const std::string& path2)
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

        Ref<VertexArray> GetActiveVAO(const Ref<StaticMesh>& mesh, int entityID, Scene* scene, Ref<SkeletalMesh>& outSkeletalMesh)
        {
            Ref<VertexArray> targetVAO = mesh->GetVertexArray();
            outSkeletalMesh = nullptr;
            if (scene && entityID >= 0)
            {
                Entity entity = { (entt::entity)entityID, scene };
                static const std::string s_EmptyPath;
                const std::string* childPath = &s_EmptyPath;
                if (entity.HasComponent<StaticMeshComponent>())
                {
                    childPath = &entity.GetComponent<StaticMeshComponent>().AssetPath;
                }

                Entity animatorEntity = entity;
                while (animatorEntity)
                {
                    if (animatorEntity.HasComponent<AnimatorComponent>())
                    {
                        auto& animator = animatorEntity.GetComponent<AnimatorComponent>();
                        if (animator.SkinnedVAO && animator.MeshAsset)
                        {
                            if (PathsMatch(*childPath, animator.MeshAssetPath))
                            {
                                targetVAO = animator.SkinnedVAO;
                                outSkeletalMesh = animator.MeshAsset;
                            }
                        }
                        break;
                    }

                    if (animatorEntity.HasComponent<RelationshipComponent>())
                    {
                        UUID parentHandle = animatorEntity.GetComponent<RelationshipComponent>().ParentHandle;
                        if (parentHandle != 0)
                        {
                            animatorEntity = scene->GetEntityByUUID(parentHandle);
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
            return targetVAO;
        }

        Ref<VertexArray> GetActiveVAO(const Ref<StaticMesh>& mesh, int entityID, Scene* scene)
        {
            Ref<SkeletalMesh> dummy;
            return GetActiveVAO(mesh, entityID, scene, dummy);
        }
    }

    struct LightDataGPU
    {
        glm::vec4 DirLightDirection;
        glm::vec4 DirLightColor;

        uint32_t PointLightCount;
        uint32_t SpotLightCount;

        float EnvironmentIntensity;
        uint32_t Padding; // align to 16 bytes

        struct PointLightGPU
        {
            glm::vec4 Position;  // w = Intensity
            glm::vec4 Color;     // w = Radius
            glm::vec4 Falloff;   // x = Falloff, yzw = Padding
            glm::vec4 ExtraData; // x = ShadowIndex (-1 if disabled)
        };

        struct SpotLightGPU
        {
            glm::vec4 Position;  // w = Intensity
            glm::vec4 Direction; // w = Radius
            glm::vec4 Color;     // w = Inner CutOff
            glm::vec4 Falloff;   // x = Falloff, y = Outer Cutoff, w = Texture size
            glm::mat4 LightSpaceMatrix;
            glm::vec4 ExtraData; // x = ShadowIndex (-1 if disabled)
        };
    };

    struct ShadowData
    {
        Ref<ShadowMap> ShadowTarget;
        Ref<Shader> ShadowShader;
        Ref<UniformBuffer> ShadowUniformBuffer;

        struct ShadowDataGPU
        {
            glm::mat4 LightSpaceMatrices[4];
            glm::vec4 CascadePlaneDistances[4];

            float LightSize;
            float ContactThreshold;
            float ContactSharpness;
            float ContactSharpeningBias;
            float SoftShadows;
            float Padding[3];
        } BufferLocal;

        std::vector<float> CascadeSplits;

        // WP18: far-cascade staggering. Cascades 0/1 refresh every frame,
        // cascade 2 on even frames, cascade 3 on odd frames. A skipped cascade
        // keeps BOTH its depth layer (never cleared) and its matrix in
        // BufferLocal, so sampling stays perfectly consistent.
        uint32_t FrameIndex = 0;
        bool CascadeNeedsRender[4] = { true, true, true, true };
        bool ForceAllCascades = true;
        glm::vec3 LastLightDir = glm::vec3(0.0f);
        glm::vec3 LastCameraPos = glm::vec3(0.0f);
    };

    struct LineVertex
    {
        glm::vec3 Position;
        glm::vec4 Color;
    };

    static constexpr uint32_t MaxInstances = 10000;

    struct RendererData
    {
        Ref<RenderTarget> CurrentRenderTarget = nullptr;
        std::vector<RenderCommandPacket> OpaqueQueue;
        std::vector<RenderCommandPacket> TransparentQueue;
        std::vector<RenderCommandPacket> ShadowQueue;

        Ref<ShadowMap> SpotShadowTarget;
        Ref<ShadowMap> PointShadowTarget;
        Ref<Shader> PointShadowShader;

        std::vector<InstanceData> BatchBuffer;

        glm::mat4 ViewProjectionMatrix;
        glm::mat4 PrevViewProjectionMatrix = glm::mat4(1.0f);
        glm::mat4 ViewMatrix;
        glm::vec3 CameraPosition;
        glm::vec3 CameraForward;
        Frustum CameraFrustum;
        float CameraFOV = 45.0f;

        uint32_t CurrentShaderID = 0;
        std::vector<RendererAPI::TextureUnitBinding> DebugTextureBindings; // WP10 state inspector
        bool RenderingTransparent = false; // true while drawing the blended transparent queue; gates G-buffer (normal/SSR) writes
        uint32_t CurrentVertexArrayID = 0;

        std::array<uint32_t, 32> TextureSlots{ 0 };

        Ref<StreamVertexBuffer> InstanceVertexBuffer;
        glm::mat4* InstanceBufferBase = nullptr;
        uint32_t InstanceCount = 0;

        Ref<UniformBuffer> LightUniformBuffer;
        LightDataGPU LightBufferLocal;
        Ref<LightCuller> Clusters;
        Ref<Texture2D> BlueNoiseTexture;
        bool ClusteredLightingEnabled = true;
        Scene* ActiveScene = nullptr;
        std::vector<LightDataGPU::PointLightGPU> PointLightsGPU;
        std::vector<LightDataGPU::SpotLightGPU> SpotLightsGPU;
        std::vector<int> PointLightEntityIDs;
        std::vector<int> SpotLightEntityIDs;

        Ref<VertexArray> SkyboxVAO;
        Ref<Shader> SkyboxShader;

        Ref<Cubemap> SceneEnvironment;
        ShadowData ShadowData;

        Ref<VertexArray> LineVAO;
        Ref<VertexBuffer> LineVBO;
        Ref<Shader> LineShader;

        std::vector<LineVertex> LineVertices;
        const uint32_t MaxLineVertices = 100000;

        struct AmbientVolume
        {
            glm::mat4 InverseTransform;
            glm::vec3 HalfExtents;
            float Intensity;
            glm::vec3 TransitionMin;
            glm::vec3 TransitionMax;
        };
        std::vector<AmbientVolume> ActiveAmbientVolumes;

        struct ReflectionProbe
        {
            glm::mat4 Transform;
            glm::mat4 InverseTransform;
            glm::vec3 HalfExtents;
            float BlendDistance;
            float Intensity;
        };
        std::vector<ReflectionProbe> ActiveReflectionProbes;

        RendererStatistics Stats;
        bool DirLightCastsShadows = true;
    };

    RendererStatistics Renderer::GetStats()
    {
        return m_Data->Stats;
    }

    void Renderer::ResetStats()
    {
        m_Data->Stats.Reset();
    }

    std::vector<glm::vec4> GetFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view)
    {
        RXN_PROFILE_SCOPE();

        const auto inv = glm::inverse(proj * view);
        std::vector<glm::vec4> frustumCorners;
        for (unsigned int x = 0; x < 2; ++x)
        {
            for (unsigned int y = 0; y < 2; ++y)
            {
                for (unsigned int z = 0; z < 2; ++z)
                {
                    const glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
                    frustumCorners.push_back(pt / pt.w);
                }
            }
        }
        return frustumCorners;
    }

    void Renderer::CalculateShadowMapMatrices(const glm::mat4& cameraView, const glm::vec3& lightDir)
    {
        RXN_PROFILE_SCOPE();

        float aspect = (float)m_Data->CurrentRenderTarget->GetSpecification().Width / (float)m_Data->CurrentRenderTarget->GetSpecification().Height;
        float fov = m_Data->CameraFOV < 0.01f ? glm::radians(45.0f) : m_Data->CameraFOV;
        float nearPlane = 0.1f;
        float maxShadowDistance = 150.0f;

        float lambda = 0.92f;
        m_Data->ShadowData.CascadeSplits.resize(4);
        for (size_t i = 0; i < 4; ++i)
        {
            float p = (float)(i + 1) / 4.0f;
            float logSplit = nearPlane * std::pow(maxShadowDistance / nearPlane, p);
            float linSplit = nearPlane + p * (maxShadowDistance - nearPlane);
            m_Data->ShadowData.CascadeSplits[i] = lambda * logSplit + (1.0f - lambda) * linSplit;
        }

        // WP18: decide which cascades refresh this frame. Near cascades (0/1)
        // update every frame; far cascades (2/3) alternate frames. Any
        // discontinuity (light rotated, camera teleported/cut to a different
        // view, shadow map reallocated, first frame) forces a full refresh so
        // a stale layer is never sampled with a mismatched matrix.
        glm::vec3 lightDirNorm = glm::normalize(lightDir);
        glm::vec3 cameraPos = glm::vec3(glm::inverse(cameraView)[3]);

        bool forceAll = m_Data->ShadowData.ForceAllCascades;
        if (glm::dot(lightDirNorm, m_Data->ShadowData.LastLightDir) < 0.99995f)
            forceAll = true; // light rotated (e.g. time-of-day sun)
        if (glm::distance(cameraPos, m_Data->ShadowData.LastCameraPos) > 2.5f)
            forceAll = true; // teleport / camera cut / probe or PIP view

        m_Data->ShadowData.LastLightDir = lightDirNorm;
        m_Data->ShadowData.LastCameraPos = cameraPos;
        m_Data->ShadowData.ForceAllCascades = false;

        const uint32_t parity = m_Data->ShadowData.FrameIndex & 1u;
        m_Data->ShadowData.FrameIndex++;

        m_Data->ShadowData.CascadeNeedsRender[0] = true;
        m_Data->ShadowData.CascadeNeedsRender[1] = true;
        m_Data->ShadowData.CascadeNeedsRender[2] = forceAll || (parity == 0u);
        m_Data->ShadowData.CascadeNeedsRender[3] = forceAll || (parity == 1u);

        for (size_t i = 0; i < 4; ++i)
        {
            // WP18: a skipped cascade keeps last frame's depth AND matrix. Do
            // NOT recompute the matrix here, or the stored depth map would no
            // longer match what the lighting pass samples with.
            if (!m_Data->ShadowData.CascadeNeedsRender[i])
                continue;

            float pNear = (i == 0) ? nearPlane : m_Data->ShadowData.CascadeSplits[i - 1];
            float pFar = m_Data->ShadowData.CascadeSplits[i];

            glm::mat4 proj = glm::perspective(fov, aspect, pNear, pFar);
            auto corners = GetFrustumCornersWorldSpace(proj, cameraView);

            glm::vec3 center = glm::vec3(0, 0, 0);
            for (const auto& v : corners)
                center += glm::vec3(v);

            center /= corners.size();

            float radius = 0.0f;
            for (const auto& v : corners) radius = std::max(radius, glm::distance(glm::vec3(v), center));
            radius = std::ceil(radius * 16.0f) / 16.0f;

            glm::vec3 maxExtents = glm::vec3(radius);
            glm::vec3 minExtents = -maxExtents;

            glm::vec3 lightDirectionNormalized = glm::normalize(lightDir);
            glm::vec3 shadowCameraPos = center - lightDirectionNormalized * radius;
            glm::mat4 lightViewMatrix = glm::lookAt(shadowCameraPos, center, glm::vec3(0.0f, 1.0f, 0.0f));

            float shadowMapRes = (float)m_Data->ShadowData.ShadowTarget->GetSize();
            float worldTexelSize = (radius * 2.0f) / shadowMapRes;

            glm::vec4 lightSpaceCenter = lightViewMatrix * glm::vec4(center, 1.0f);
            lightSpaceCenter.x = std::floor(lightSpaceCenter.x / worldTexelSize) * worldTexelSize;
            lightSpaceCenter.y = std::floor(lightSpaceCenter.y / worldTexelSize) * worldTexelSize;

            glm::mat4 invLightView = glm::inverse(lightViewMatrix);
            glm::vec4 snappedCenter = invLightView * lightSpaceCenter;

            lightViewMatrix = glm::lookAt(glm::vec3(snappedCenter) - lightDirectionNormalized * radius, glm::vec3(snappedCenter), glm::vec3(0.0f, 1.0f, 0.0f));

            float minX = minExtents.x;
            float maxX = maxExtents.x;
            float minY = minExtents.y;
            float maxY = maxExtents.y;

            float zPadding = 50.0f;
            if (i == 0)
                zPadding = 15.0f;  // Cascade 0: 15m
            else if (i == 1)
                zPadding = 35.0f;  // Cascade 1: 35m
            else if (i == 2)
                zPadding = 80.0f;  // Cascade 2: 80m
            else             
                zPadding = 200.0f; // Cascade 3: 200m

            float minZ = minExtents.z - zPadding;
            float maxZ = maxExtents.z + zPadding;

            const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);

            m_Data->ShadowData.BufferLocal.LightSpaceMatrices[i] = lightProjection * lightViewMatrix;
            m_Data->ShadowData.BufferLocal.CascadePlaneDistances[i].x = pFar;
            m_Data->ShadowData.BufferLocal.CascadePlaneDistances[i].y = worldTexelSize;
            m_Data->ShadowData.BufferLocal.CascadePlaneDistances[i].z = maxZ - minZ;
            m_Data->ShadowData.BufferLocal.CascadePlaneDistances[i].w = radius * 2.0f;
        }

        m_Data->ShadowData.ShadowUniformBuffer->SetData(&m_Data->ShadowData.BufferLocal, sizeof(ShadowData::ShadowDataGPU));
    }

    void Renderer::Init()
    {
        m_Data = new RendererData();
        m_Data->BatchBuffer.resize(MaxInstances);

        RenderCommand::Init();
        Renderer2D::Init();

        m_Data->OpaqueQueue.reserve(1000);
        m_Data->InstanceVertexBuffer = StreamVertexBuffer::Create(MaxInstances * sizeof(InstanceData));
        m_Data->InstanceVertexBuffer->SetLayout({
                { ShaderDataType::Float4, "a_ModelMatrixCol0", false, true },
                { ShaderDataType::Float4, "a_ModelMatrixCol1", false, true },
                { ShaderDataType::Float4, "a_ModelMatrixCol2", false, true },
                { ShaderDataType::Float4, "a_ModelMatrixCol3", false, true },
                { ShaderDataType::Int,    "a_EntityID",        false, true }
            });
        m_Data->LightUniformBuffer = UniformBuffer::Create(sizeof(LightDataGPU), 1);
        m_Data->Clusters = CreateRef<LightCuller>();
        m_Data->Clusters->Init();
        m_Data->BlueNoiseTexture = Texture2D::Create("res/textures/bluenoise_rg_64.png");

        float skyboxVertices[] = {
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f
        };

        Ref<VertexBuffer> skyboxVB = VertexBuffer::Create(skyboxVertices, sizeof(skyboxVertices));
        skyboxVB->SetLayout({ { ShaderDataType::Float3, "a_Position" } });
        m_Data->SkyboxVAO = VertexArray::Create();
        m_Data->SkyboxVAO->AddVertexBuffer(skyboxVB);

        m_Data->SkyboxShader = Shader::Create("res/shaders/skybox.glsl");

        m_Data->SpotShadowTarget = ShadowMap::Create();
        m_Data->SpotShadowTarget->Init(1024, 4, false);

        m_Data->PointShadowTarget = ShadowMap::Create();
        m_Data->PointShadowTarget->Init(1024, 4, true);
        m_Data->PointShadowShader = Shader::Create("res/shaders/point_shadow_depth.glsl");

        m_Data->ShadowData.ShadowTarget = ShadowMap::Create();
        m_Data->ShadowData.ShadowTarget->Init(2048, 4, false);
        m_Data->ShadowData.ShadowShader = Shader::Create("res/shaders/shadow_depth.glsl");

        m_Data->ShadowData.ShadowUniformBuffer = UniformBuffer::Create(sizeof(ShadowData::ShadowDataGPU), 2);

        m_Data->ShadowData.CascadeSplits = { 7.0f, 25.0f, 90.0f, 1000.0f };

        m_Data->LineVAO = VertexArray::Create();
        m_Data->LineVBO = VertexBuffer::Create(m_Data->MaxLineVertices * sizeof(LineVertex));
        m_Data->LineVBO->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float4, "a_Color" }
            });
        m_Data->LineVAO->AddVertexBuffer(m_Data->LineVBO);
        m_Data->LineShader = Shader::Create("res/shaders/line.glsl");
    }

    void Renderer::Shutdown()
    {
        Renderer2D::Shutdown();
        m_Data->OpaqueQueue.clear();
        delete m_Data;
        m_Data = nullptr;
    }

    void Renderer::OnWindowResize(uint32_t width, uint32_t height)
    {
        RenderCommand::SetViewport(0, 0, width, height);
    }

    void Renderer::BeginScene(const EditorCamera& camera, const LightEnvironment& lights,
        const Ref<Cubemap>& environment, const Ref<RenderTarget>& renderTarget, Scene* scene, const glm::mat4& prevViewProj)
    {
        RXN_PROFILE_SCOPE();
        PrepareScene(camera.GetViewProjection(), camera.GetViewMatrix(), camera.GetPosition(), camera.GetFOV(), lights, environment, renderTarget, scene, prevViewProj);
    }

    void Renderer::BeginScene(const Camera& camera, const glm::mat4& transform, const LightEnvironment& lights,
        const Ref<Cubemap>& environment, const Ref<RenderTarget>& renderTarget, Scene* scene, const glm::mat4& prevViewProj)
    {
        RXN_PROFILE_SCOPE();

        glm::mat4 viewProj = camera.GetProjection() * glm::inverse(transform);
        glm::mat4 view = glm::inverse(transform);
        glm::vec3 cameraPos = glm::vec3(transform[3]);
        float fov = 2.0f * glm::atan(1.0f / camera.GetProjection()[1][1]);

        PrepareScene(viewProj, view, cameraPos, fov, lights, environment, renderTarget, scene, prevViewProj);
    }

    void Renderer::PrepareScene(
        const glm::mat4& viewProjection,
        const glm::mat4& viewMatrix,
        const glm::vec3& cameraPosition,
        float cameraFOV,
        const LightEnvironment& lights,
        const Ref<Cubemap>& environment,
        const Ref<RenderTarget>& renderTarget,
        Scene* scene,
        const glm::mat4& prevViewProj)
    {
        RXN_PROFILE_SCOPE();
        m_Data->ActiveScene = scene;

        m_Data->ActiveAmbientVolumes.clear();
        if (scene)
        {
            auto boxView = scene->GetRaw().view<BoxColliderComponent, TransformComponent>();
            for (auto entity : boxView)
            {
                const auto& bc = boxView.get<BoxColliderComponent>(entity);
                if (bc.IsAmbientZone)
                {
                    glm::mat4 worldTransform = scene->GetWorldTransform({ entity, scene });
                    glm::mat4 offsetTransform = glm::translate(worldTransform, bc.Offset);
                    
                    RendererData::AmbientVolume vol;
                    vol.InverseTransform = glm::inverse(offsetTransform);
                    vol.HalfExtents = bc.HalfExtents;
                    vol.Intensity = bc.AmbientIntensity;
                    vol.TransitionMin = bc.TransitionMin;
                    vol.TransitionMax = bc.TransitionMax;
                    m_Data->ActiveAmbientVolumes.push_back(vol);
                    
                    if (m_Data->ActiveAmbientVolumes.size() >= 8)
                        break;
                }
            }
        }

        m_Data->ActiveReflectionProbes.clear();
        if (scene)
        {
            auto probeView = scene->GetRaw().view<ReflectionProbeComponent, TransformComponent>();
            for (auto entity : probeView)
            {
                const auto& pc = probeView.get<ReflectionProbeComponent>(entity);
                if (!pc.Active)
                    continue;

                glm::mat4 worldTransform = scene->GetWorldTransform({ entity, scene });

                RendererData::ReflectionProbe probe;
                probe.Transform = worldTransform;
                probe.InverseTransform = glm::inverse(worldTransform);
                probe.HalfExtents = pc.BoxHalfExtents;
                probe.BlendDistance = pc.BlendDistance;
                probe.Intensity = pc.Intensity;
                m_Data->ActiveReflectionProbes.push_back(probe);

                if (m_Data->ActiveReflectionProbes.size() >= 4)
                    break;
            }
        }

        if (m_Data->BlueNoiseTexture)
            RenderCommand::BindTextureID(25, m_Data->BlueNoiseTexture->GetRendererID());

        if (environment)
        {
            m_Data->SceneEnvironment = environment;

            RenderCommand::BindTextureID(10, environment->GetIrradianceRendererID());
            RenderCommand::BindTextureID(11, environment->GetPrefilterRendererID());
            RenderCommand::BindTextureID(12, environment->GetBRDFLUTRendererID());
        }
        else
        {
            m_Data->SceneEnvironment = nullptr;
        }

        m_Data->CurrentRenderTarget = renderTarget;

        if (m_Data->CurrentRenderTarget)
        {
            m_Data->CurrentRenderTarget->Bind();
            RenderCommand::SetViewport(0, 0,
                m_Data->CurrentRenderTarget->GetSpecification().Width,
                m_Data->CurrentRenderTarget->GetSpecification().Height);
            RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
            RenderCommand::Clear();
        }
        else
        {
            RenderCommand::BindDefaultRenderTarget();
            RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 1.0f });
            RenderCommand::Clear();
        }

        m_Data->ViewProjectionMatrix = viewProjection;
        m_Data->PrevViewProjectionMatrix = prevViewProj;
        m_Data->ViewMatrix = viewMatrix;
        m_Data->CameraPosition = cameraPosition;
        m_Data->CameraForward = -glm::normalize(glm::vec3(glm::inverse(viewMatrix)[2]));
        m_Data->CameraFrustum.Define(viewProjection);

        m_Data->LightBufferLocal.DirLightDirection = glm::vec4(lights.DirLight.Direction, lights.DirLight.Intensity);
        m_Data->LightBufferLocal.DirLightColor = glm::vec4(lights.DirLight.Color, 0.0f);
        m_Data->LightBufferLocal.PointLightCount = (uint32_t)lights.PointLights.size();
        m_Data->PointLightsGPU.resize(m_Data->LightBufferLocal.PointLightCount);
        m_Data->PointLightEntityIDs.assign(m_Data->LightBufferLocal.PointLightCount, -1);

        uint32_t activeSpotShadows = 0;
        uint32_t activePointShadows = 0;

        for (uint32_t i = 0; i < m_Data->LightBufferLocal.PointLightCount; i++)
        {
            const auto& light = lights.PointLights[i];
            m_Data->PointLightsGPU[i].Position = glm::vec4(light.Position, light.Intensity);
            m_Data->PointLightsGPU[i].Color = glm::vec4(light.Color, light.Radius);
            m_Data->PointLightsGPU[i].Falloff = glm::vec4(light.Falloff, 0.0f, 0.0f, 0.0f);

            float shadowIndex = -1.0f;
            bool isVisibleToCamera = m_Data->CameraFrustum.IsSphereVisible(light.Position, light.Radius);

            if (light.CastsShadows && isVisibleToCamera && activePointShadows < 4)
                shadowIndex = (float)activePointShadows++;

            m_Data->PointLightsGPU[i].ExtraData = glm::vec4(shadowIndex, 0.0f, 0.0f, 0.0f);
            m_Data->PointLightEntityIDs[i] = light.EntityID;
        }


        m_Data->LightBufferLocal.SpotLightCount = (uint32_t)lights.SpotLights.size();
        m_Data->SpotLightsGPU.resize(m_Data->LightBufferLocal.SpotLightCount);
        m_Data->SpotLightEntityIDs.assign(m_Data->LightBufferLocal.SpotLightCount, -1);
        m_Data->LightBufferLocal.EnvironmentIntensity = lights.EnvironmentIntensity;

        uint32_t currentCookieSlot = 16;

        for (uint32_t i = 0; i < m_Data->LightBufferLocal.SpotLightCount; i++)
        {
            const auto& light = lights.SpotLights[i];

            int cookieIndex = -1;
            if (light.CookieTexture && currentCookieSlot < 24)
            {
                RenderCommand::BindTextureID(currentCookieSlot, light.CookieTexture->GetRendererID());
                cookieIndex = currentCookieSlot;
                currentCookieSlot++;
            }

            m_Data->SpotLightsGPU[i].Position = glm::vec4(light.Position, light.Intensity);
            m_Data->SpotLightsGPU[i].Direction = glm::vec4(light.Direction, light.Radius);
            m_Data->SpotLightsGPU[i].Color = glm::vec4(light.Color, light.CutOff);
            m_Data->SpotLightsGPU[i].Falloff = glm::vec4(light.Falloff, light.OuterCutOff, (float)cookieIndex, light.CookieSize);
            m_Data->SpotLightsGPU[i].LightSpaceMatrix = light.LightSpaceMatrix;

            float shadowIndex = -1.0f;
            bool isVisibleToCamera = m_Data->CameraFrustum.IsSphereVisible(light.Position, light.Radius);

            if (light.CastsShadows && isVisibleToCamera && activeSpotShadows < 4)
                shadowIndex = (float)activeSpotShadows++;

            m_Data->SpotLightsGPU[i].ExtraData = glm::vec4(shadowIndex, 0.0f, 0.0f, 0.0f);
            m_Data->SpotLightEntityIDs[i] = light.EntityID;
        }

        m_Data->LightUniformBuffer->SetData(&m_Data->LightBufferLocal, sizeof(LightDataGPU));

        // Clustered Forward+ : build the froxel grid and cull this frame's lights.
        if (m_Data->ClusteredLightingEnabled)
        {
            uint32_t clusterScreenW = renderTarget ? renderTarget->GetSpecification().Width : 1280;
            uint32_t clusterScreenH = renderTarget ? renderTarget->GetSpecification().Height : 720;
            m_Data->Clusters->BuildAndCull(
                viewProjection, viewMatrix, clusterScreenW, clusterScreenH,
                m_Data->PointLightsGPU.data(), m_Data->LightBufferLocal.PointLightCount, (uint32_t)sizeof(LightDataGPU::PointLightGPU),
                m_Data->SpotLightsGPU.data(),  m_Data->LightBufferLocal.SpotLightCount,  (uint32_t)sizeof(LightDataGPU::SpotLightGPU));
        }

        m_Data->OpaqueQueue.clear();
        m_Data->TransparentQueue.clear();
        m_Data->ShadowQueue.clear();

        m_Data->CurrentShaderID = 0;
        m_Data->CurrentVertexArrayID = 0;
        std::fill(m_Data->TextureSlots.begin(), m_Data->TextureSlots.end(), 0);
        m_Data->LineVertices.clear();

        m_Data->ShadowData.BufferLocal.LightSize = lights.ShadowLightSize;
        m_Data->ShadowData.BufferLocal.ContactThreshold = lights.ShadowContactThreshold;
        m_Data->ShadowData.BufferLocal.ContactSharpness = lights.ShadowContactSharpness;
        m_Data->ShadowData.BufferLocal.ContactSharpeningBias = lights.ShadowContactSharpeningBias;
        m_Data->ShadowData.BufferLocal.SoftShadows = lights.SoftShadows ? 1.0f : 0.0f;

        m_Data->DirLightCastsShadows = lights.DirLight.CastsShadows;

        // Dynamic Resizing for Point Lights Shadow Map Array:
        uint32_t maxPointRes = 16;
        for (uint32_t i = 0; i < m_Data->LightBufferLocal.PointLightCount; i++)
        {
            const auto& light = lights.PointLights[i];
            if (light.CastsShadows)
            {
                maxPointRes = std::max(maxPointRes, light.ShadowResolution);
            }
        }
        if (maxPointRes != m_Data->PointShadowTarget->GetSize())
        {
            m_Data->PointShadowTarget->Init(maxPointRes, 4, true);
        }

        // Dynamic Resizing for Spot Lights Shadow Map Array:
        uint32_t maxSpotRes = 16;
        for (uint32_t i = 0; i < m_Data->LightBufferLocal.SpotLightCount; i++)
        {
            const auto& light = lights.SpotLights[i];
            if (light.CastsShadows)
            {
                maxSpotRes = std::max(maxSpotRes, light.ShadowResolution);
            }
        }
        if (maxSpotRes != m_Data->SpotShadowTarget->GetSize())
        {
            m_Data->SpotShadowTarget->Init(maxSpotRes, 4, false);
        }

        // Dynamic Resizing for CSM / Directional Lights Shadow Map Array:
        uint32_t dirRes = 16;
        if (lights.DirLight.CastsShadows)
        {
            dirRes = lights.DirLight.ShadowResolution;
        }
        if (dirRes != m_Data->ShadowData.ShadowTarget->GetSize())
        {
            m_Data->ShadowData.ShadowTarget->Init(dirRes, 4, false);
            m_Data->ShadowData.ForceAllCascades = true; // WP18: layers reallocated, contents lost
        }

        CalculateShadowMapMatrices(m_Data->ViewMatrix, m_Data->LightBufferLocal.DirLightDirection);
    }

    void Renderer::Submit(const Ref<StaticMesh>& mesh, uint32_t submeshIndex, const Ref<Material>& material, const glm::mat4& transform, int entityID, uint32_t lodIndex)
    {
        RXN_PROFILE_SCOPE();

        const auto& submeshes = mesh->GetSubmeshes();

        if (submeshIndex >= submeshes.size()) 
            return;

        RenderCommandPacket packet;
        packet.Mesh = mesh;
        packet.SubmeshIndex = submeshIndex;
        packet.Material = material;
        packet.Transform = transform;
        packet.EntityID = entityID;
        packet.LODIndex = lodIndex;

        glm::vec3 position = glm::vec3(transform[3]);
        glm::vec3 directionToPosition = position - m_Data->CameraPosition;
        packet.DistanceToCamera = glm::dot(m_Data->CameraForward, directionToPosition);

        // Guard: a submesh whose material slot is unassigned yields an empty Ref<Material>
        // (from GetMaterials()[matIndex] at the call site). Dereferencing material->GetShader()
        // below would crash with an access violation reading 0x8. Skip the draw instead.
        if (!material || !material->GetShader())
        {
            static bool s_WarnedNullSubmitMaterial = false;
            if (!s_WarnedNullSubmitMaterial)
            {
                RXN_CORE_WARN("Renderer::Submit: entity {0} submesh {1} has no material/shader assigned; skipping draw. Assign a material to this mesh slot.", entityID, submeshIndex);
                s_WarnedNullSubmitMaterial = true;
            }
            return;
        }

        // WP3: resolve the active VAO (skinned or static) once per packet so
        // queue execution never walks the scene graph per draw.
        Ref<SkeletalMesh> submitSkeletalMesh = nullptr;
        packet.ResolvedVAO = GetActiveVAO(mesh, entityID, m_Data->ActiveScene, submitSkeletalMesh);
        packet.ResolvedSkeletalMesh = submitSkeletalMesh;

        uint64_t shaderID = material->GetShader()->GetRendererID();
        uint64_t vaoID = packet.ResolvedVAO->GetRendererID();

        packet.SortKey = (shaderID << 32) | ((vaoID & 0xFFFF) << 16) | ((submeshIndex & 0xFF) << 8) | (lodIndex & 0xFF);

        if (material->IsTransparent())
            m_Data->TransparentQueue.push_back(packet);
        else
            m_Data->OpaqueQueue.push_back(packet);
    }

    const std::vector<RendererAPI::TextureUnitBinding>& Renderer::GetDebugTextureBindings() const
    {
        return m_Data->DebugTextureBindings;
    }

    void Renderer::EndScene()
    {
        Flush();

        if (m_Data->CurrentRenderTarget)
        {
            m_Data->CurrentRenderTarget->Unbind();
            m_Data->CurrentRenderTarget = nullptr;
        }
    }

    void Renderer::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color)
    {
        if (m_Data->LineVertices.size() >= m_Data->MaxLineVertices)
            return;

        m_Data->LineVertices.push_back({ p0, color });
        m_Data->LineVertices.push_back({ p1, color });
    }

    void Renderer::DrawWireBox(const glm::mat4& transform, const glm::vec4& color)
    {
        RXN_PROFILE_SCOPE();

        glm::vec3 lineVertices[8];
        for (size_t i = 0; i < 8; i++)
        {
            float x = (i & 1) ? 0.5f : -0.5f;
            float y = (i & 2) ? 0.5f : -0.5f;
            float z = (i & 4) ? 0.5f : -0.5f;
            lineVertices[i] = transform * glm::vec4(x, y, z, 1.0f);
        }

        // bottom face
        DrawLine(lineVertices[0], lineVertices[1], color);
        DrawLine(lineVertices[1], lineVertices[3], color);
        DrawLine(lineVertices[3], lineVertices[2], color);
        DrawLine(lineVertices[2], lineVertices[0], color);

        // top face
        DrawLine(lineVertices[4], lineVertices[5], color);
        DrawLine(lineVertices[5], lineVertices[7], color);
        DrawLine(lineVertices[7], lineVertices[6], color);
        DrawLine(lineVertices[6], lineVertices[4], color);

        // pillars
        DrawLine(lineVertices[0], lineVertices[4], color);
        DrawLine(lineVertices[1], lineVertices[5], color);
        DrawLine(lineVertices[2], lineVertices[6], color);
        DrawLine(lineVertices[3], lineVertices[7], color);
    }

    void Renderer::DrawWireSphere(const glm::mat4& transform, const glm::vec4& color)
    {
        RXN_PROFILE_SCOPE();

        const int segments = 32;
        constexpr float angleStep = glm::two_pi<float>() / segments;

        for (int i = 0; i < segments; i++)
        {
            float a0 = i * angleStep;
            float a1 = (i + 1) * angleStep;

            glm::vec3 p0_xy = glm::vec3(glm::cos(a0), glm::sin(a0), 0.0f);
            glm::vec3 p1_xy = glm::vec3(glm::cos(a1), glm::sin(a1), 0.0f);
            DrawLine(transform * glm::vec4(p0_xy, 1.0f), transform * glm::vec4(p1_xy, 1.0f), color);

            glm::vec3 p0_xz = glm::vec3(glm::cos(a0), 0.0f, glm::sin(a0));
            glm::vec3 p1_xz = glm::vec3(glm::cos(a1), 0.0f, glm::sin(a1));
            DrawLine(transform * glm::vec4(p0_xz, 1.0f), transform * glm::vec4(p1_xz, 1.0f), color);

            glm::vec3 p0_yz = glm::vec3(0.0f, glm::cos(a0), glm::sin(a0));
            glm::vec3 p1_yz = glm::vec3(0.0f, glm::cos(a1), glm::sin(a1));
            DrawLine(transform * glm::vec4(p0_yz, 1.0f), transform * glm::vec4(p1_yz, 1.0f), color);
        }
    }

    void Renderer::DrawWireCapsule(const glm::mat4& transform, float radius, float height, const glm::vec4& color)
    {
        RXN_PROFILE_SCOPE();

        const int segments = 32;
        const float angleStep = glm::two_pi<float>() / segments;
        const float halfHeight = height / 2.0f;

        DrawLine(transform * glm::vec4(radius, halfHeight, 0.0f, 1.0f), transform * glm::vec4(radius, -halfHeight, 0.0f, 1.0f), color);
        DrawLine(transform * glm::vec4(-radius, halfHeight, 0.0f, 1.0f), transform * glm::vec4(-radius, -halfHeight, 0.0f, 1.0f), color);
        DrawLine(transform * glm::vec4(0.0f, halfHeight, radius, 1.0f), transform * glm::vec4(0.0f, -halfHeight, radius, 1.0f), color);
        DrawLine(transform * glm::vec4(0.0f, halfHeight, -radius, 1.0f), transform * glm::vec4(0.0f, -halfHeight, -radius, 1.0f), color);

        for (int i = 0; i < segments; i++)
        {
            float a0 = i * angleStep;
            float a1 = (i + 1) * angleStep;

            glm::vec3 p0_top = glm::vec3(glm::cos(a0) * radius, halfHeight, glm::sin(a0) * radius);
            glm::vec3 p1_top = glm::vec3(glm::cos(a1) * radius, halfHeight, glm::sin(a1) * radius);
            DrawLine(transform * glm::vec4(p0_top, 1.0f), transform * glm::vec4(p1_top, 1.0f), color);

            glm::vec3 p0_bot = glm::vec3(glm::cos(a0) * radius, -halfHeight, glm::sin(a0) * radius);
            glm::vec3 p1_bot = glm::vec3(glm::cos(a1) * radius, -halfHeight, glm::sin(a1) * radius);
            DrawLine(transform * glm::vec4(p0_bot, 1.0f), transform * glm::vec4(p1_bot, 1.0f), color);

            if (i < segments / 2)
            {
                float ha0 = a0;
                float ha1 = a1;

                glm::vec3 p0_xy_top = glm::vec3(glm::cos(ha0) * radius, glm::sin(ha0) * radius + halfHeight, 0.0f);
                glm::vec3 p1_xy_top = glm::vec3(glm::cos(ha1) * radius, glm::sin(ha1) * radius + halfHeight, 0.0f);
                DrawLine(transform * glm::vec4(p0_xy_top, 1.0f), transform * glm::vec4(p1_xy_top, 1.0f), color);

                glm::vec3 p0_yz_top = glm::vec3(0.0f, glm::sin(ha0) * radius + halfHeight, glm::cos(ha0) * radius);
                glm::vec3 p1_yz_top = glm::vec3(0.0f, glm::sin(ha1) * radius + halfHeight, glm::cos(ha1) * radius);
                DrawLine(transform * glm::vec4(p0_yz_top, 1.0f), transform * glm::vec4(p1_yz_top, 1.0f), color);

                float ba0 = ha0 + glm::pi<float>();
                float ba1 = ha1 + glm::pi<float>();

                glm::vec3 p0_xy_bot = glm::vec3(glm::cos(ba0) * radius, glm::sin(ba0) * radius - halfHeight, 0.0f);
                glm::vec3 p1_xy_bot = glm::vec3(glm::cos(ba1) * radius, glm::sin(ba1) * radius - halfHeight, 0.0f);
                DrawLine(transform * glm::vec4(p0_xy_bot, 1.0f), transform * glm::vec4(p1_xy_bot, 1.0f), color);

                glm::vec3 p0_yz_bot = glm::vec3(0.0f, glm::sin(ba0) * radius - halfHeight, glm::cos(ba0) * radius);
                glm::vec3 p1_yz_bot = glm::vec3(0.0f, glm::sin(ba1) * radius - halfHeight, glm::cos(ba1) * radius);
                DrawLine(transform * glm::vec4(p0_yz_bot, 1.0f), transform * glm::vec4(p1_yz_bot, 1.0f), color);
            }
        }
    }

    void Renderer::DrawWireCone(const glm::mat4& transform, float length, float angleDegrees, const glm::vec4& color)
    {
        RXN_PROFILE_SCOPE();

        const int segments = 32;
        const float angleStep = glm::two_pi<float>() / segments;
        const float coneRadius = length * glm::tan(glm::radians(angleDegrees));

        for (int i = 0; i < segments; i++)
        {
            float a0 = i * angleStep;
            float a1 = (i + 1) * angleStep;

            glm::vec3 p0 = glm::vec3(glm::cos(a0) * coneRadius, glm::sin(a0) * coneRadius, -length);
            glm::vec3 p1 = glm::vec3(glm::cos(a1) * coneRadius, glm::sin(a1) * coneRadius, -length);

            DrawLine(transform * glm::vec4(p0, 1.0f), transform * glm::vec4(p1, 1.0f), color);

            if (i % (segments / 10) == 0)
                DrawLine(transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), transform * glm::vec4(p0, 1.0f), color);
        }
    }

    void Renderer::DrawArrow(const glm::mat4& transform, const glm::vec4& color)
    {
        RXN_PROFILE_SCOPE();

        DrawLine(transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), transform * glm::vec4(0.0f, 0.0f, -2.0f, 1.0f), color);

        DrawLine(transform * glm::vec4(0.0f, 0.0f, -2.0f, 1.0f), transform * glm::vec4(0.25f, 0.0f, -1.5f, 1.0f), color);
        DrawLine(transform * glm::vec4(0.0f, 0.0f, -2.0f, 1.0f), transform * glm::vec4(-0.25f, 0.0f, -1.5f, 1.0f), color);
        DrawLine(transform * glm::vec4(0.0f, 0.0f, -2.0f, 1.0f), transform * glm::vec4(0.0f, 0.25f, -1.5f, 1.0f), color);
        DrawLine(transform * glm::vec4(0.0f, 0.0f, -2.0f, 1.0f), transform * glm::vec4(0.0f, -0.25f, -1.5f, 1.0f), color);
    }

    void Renderer::DrawProjectedSphere(const glm::mat4& transform, float radius, const glm::vec4& color, Ref<Shader> shader, Ref<VertexArray> vao, uint32_t indexCount)
    {
        RenderCommand::SetCullFace(RendererAPI::CullFace::None);
        RenderCommand::SetDepthTest(false);
        RenderCommand::SetBlend(true);
        RenderCommand::SetBlendFunc(RendererAPI::BlendFactor::SrcAlpha, RendererAPI::BlendFactor::One);

        shader->Bind();
        shader->SetMat4("u_ViewProjection", m_Data->ViewProjectionMatrix);
        shader->SetMat4("u_InverseViewProjection", glm::inverse(m_Data->ViewProjectionMatrix));

        glm::mat4 model = glm::scale(transform, glm::vec3(radius));
        shader->SetMat4("u_Model", model);
        shader->SetFloat3("u_LightPos", glm::vec3(transform[3]));
        shader->SetFloat("u_Radius", radius);
        shader->SetFloat4("u_Color", color);

        vao->Bind();
        RenderCommand::DrawIndexed(vao, indexCount);

        RenderCommand::SetDepthTest(true);
        RenderCommand::SetBlendFunc(RendererAPI::BlendFactor::SrcAlpha, RendererAPI::BlendFactor::OneMinusSrcAlpha);
    }

    void Renderer::DrawProjectedCone(const glm::mat4& transform, float radius, float angle, const glm::vec3& dir, const glm::vec4& color, Ref<Shader> shader, Ref<VertexArray> vao, uint32_t indexCount)
    {
        RenderCommand::SetCullFace(RendererAPI::CullFace::None);
        RenderCommand::SetDepthTest(false);
        RenderCommand::SetBlend(true);
        RenderCommand::SetBlendFunc(RendererAPI::BlendFactor::SrcAlpha, RendererAPI::BlendFactor::One);

        shader->Bind();
        shader->SetMat4("u_ViewProjection", m_Data->ViewProjectionMatrix);
        shader->SetMat4("u_InverseViewProjection", glm::inverse(m_Data->ViewProjectionMatrix));

        float baseRadius = radius * glm::tan(glm::radians(angle));
        glm::mat4 model = glm::scale(transform, glm::vec3(baseRadius, baseRadius, radius));

        shader->SetMat4("u_Model", model);
        shader->SetFloat3("u_LightPos", glm::vec3(transform[3]));
        shader->SetFloat3("u_LightDir", dir);
        shader->SetFloat("u_Radius", radius);
        shader->SetFloat("u_CutoffCos", glm::cos(glm::radians(angle)));
        shader->SetFloat4("u_Color", color);

        vao->Bind();
        RenderCommand::DrawIndexed(vao, indexCount);

        RenderCommand::SetDepthTest(true);
        RenderCommand::SetBlendFunc(RendererAPI::BlendFactor::SrcAlpha, RendererAPI::BlendFactor::OneMinusSrcAlpha);
    }

    void Renderer::DrawFrustum(const glm::mat4& transform, const glm::mat4& projection, const glm::vec4& color)
    {
        RXN_PROFILE_SCOPE();

        glm::mat4 invProj = glm::inverse(projection);

        glm::vec3 nearCorners[4];
        glm::vec3 farCorners[4];

        glm::vec2 ndc[4] = { {-1.0f, -1.0f}, { 1.0f, -1.0f}, { 1.0f,  1.0f}, {-1.0f,  1.0f} };

        for (int i = 0; i < 4; ++i)
        {
            glm::vec4 n = invProj * glm::vec4(ndc[i], -1.0f, 1.0f);
            glm::vec4 f = invProj * glm::vec4(ndc[i], 1.0f, 1.0f);

            glm::vec3 viewNear = glm::vec3(n) / n.w;
            glm::vec3 viewFar = glm::vec3(f) / f.w;

            nearCorners[i] = glm::vec3(transform * glm::vec4(viewNear, 1.0f));
            farCorners[i] = glm::vec3(transform * glm::vec4(viewFar, 1.0f));
        }

        // Near Plane
        DrawLine(nearCorners[0], nearCorners[1], color);
        DrawLine(nearCorners[1], nearCorners[2], color);
        DrawLine(nearCorners[2], nearCorners[3], color);
        DrawLine(nearCorners[3], nearCorners[0], color);

        // Far Plane
        DrawLine(farCorners[0], farCorners[1], color);
        DrawLine(farCorners[1], farCorners[2], color);
        DrawLine(farCorners[2], farCorners[3], color);
        DrawLine(farCorners[3], farCorners[0], color);

        // Connecting Lines
        DrawLine(nearCorners[0], farCorners[0], color);
        DrawLine(nearCorners[1], farCorners[1], color);
        DrawLine(nearCorners[2], farCorners[2], color);
        DrawLine(nearCorners[3], farCorners[3], color);
    }

    void Renderer::DrawEntityOutline(const Ref<StaticMesh>& mesh, uint32_t submeshIndex, const glm::mat4& transform, const Ref<Shader>& outlineShader, Scene* scene, int entityID)
    {
        InstanceData data;
        data.Transform = transform;
        data.EntityID = entityID;

        uint32_t instanceOffset = m_Data->InstanceVertexBuffer->Push(&data, sizeof(InstanceData));

        outlineShader->Bind();

        Ref<SkeletalMesh> skeletalMesh = nullptr;
        Ref<VertexArray> targetVAO = GetActiveVAO(mesh, entityID, scene ? scene : m_Data->ActiveScene, skeletalMesh);

        targetVAO->Bind();

        uint32_t indexCount = 0;
        uint32_t baseIndex = 0;
        if (skeletalMesh && submeshIndex < skeletalMesh->GetSubmeshes().size())
        {
            const auto& submesh = skeletalMesh->GetSubmeshes()[submeshIndex];
            indexCount = submesh.IndexCount;
            baseIndex = submesh.BaseIndex;
        }
        else
        {
            const auto& submesh = mesh->GetSubmeshes()[submeshIndex];
            indexCount = submesh.IndexCount;
            baseIndex = submesh.BaseIndex;
        }

        RenderCommand::DrawIndexedInstancedStream(targetVAO, m_Data->InstanceVertexBuffer, instanceOffset, 1, indexCount, baseIndex);
    }

    void Renderer::OnSceneDestroyed(Scene* scene)
    {
        if (m_Data && m_Data->ActiveScene == scene)
            m_Data->ActiveScene = nullptr;
    }

    void Renderer::DrawSkybox(const Ref<Cubemap>& skybox, const EditorCamera& camera)
    {
        DrawSkybox(skybox, camera.GetViewMatrix(), camera.GetProjection());
    }

    void Renderer::DrawSkybox(const Ref<Cubemap>& skybox, const Camera& camera, const glm::mat4& cameraTransform)
    {
        DrawSkybox(skybox, glm::inverse(cameraTransform), camera.GetProjection());
    }

    void Renderer::DrawSkybox(const Ref<Cubemap>& skybox, const glm::mat4& cameraViewMatrix, const glm::mat4& cameraProjectionMatrix)
    {
        RXN_PROFILE_SCOPE();

        RenderCommand::SetDepthFunc(RendererAPI::DepthFunc::LessEqual);

        m_Data->SkyboxShader->Bind();

        glm::mat4 view = glm::mat4(glm::mat3(cameraViewMatrix));
        glm::mat4 projection = cameraProjectionMatrix;

        m_Data->SkyboxShader->SetMat4("u_ViewProjection", projection * view);

        skybox->Bind(0);
        m_Data->SkyboxShader->SetInt("u_Skybox", 0);

        m_Data->SkyboxVAO->Bind();
        RenderCommand::Draw(m_Data->SkyboxVAO, 36);
        m_Data->SkyboxVAO->Unbind();

        RenderCommand::SetDepthFunc(RendererAPI::DepthFunc::Less);
    }

    void Renderer::Flush()
    {
        RXN_PROFILE_SCOPE();

        // WP4: the shadow maps are still bound as sampled textures (units 8/9/13/14)
        // from the previous frame's lighting pass. Rendering into a depth texture
        // that is simultaneously bound for sampling is a framebuffer feedback loop;
        // drivers defend against it by serializing/decompressing the target per
        // draw, which starves the GPU. Unbind them before the shadow passes.
        RenderCommand::BindTextureID(8, 0);
        RenderCommand::BindTextureID(9, 0);
        RenderCommand::BindTextureID(13, 0);
        RenderCommand::BindTextureID(14, 0);

        {
            RXN_GPU_SCOPE("CSM Shadows");
            FlushShadows();
        }
        {
            RXN_GPU_SCOPE("Spot Shadows");
            FlushSpotShadows();
        }
        {
            RXN_GPU_SCOPE("Point Shadows");
            FlushPointShadows();
        }

        if (m_Data->CurrentRenderTarget)
            m_Data->CurrentRenderTarget->Bind();
        else
            RenderCommand::BindDefaultRenderTarget();

        RenderCommand::SetViewport(0, 0,
            m_Data->CurrentRenderTarget ? m_Data->CurrentRenderTarget->GetSpecification().Width : 1280,
            m_Data->CurrentRenderTarget ? m_Data->CurrentRenderTarget->GetSpecification().Height : 720);

        m_Data->ShadowData.ShadowTarget->BindRead(8);
        m_Data->SpotShadowTarget->BindRead(13);
        m_Data->PointShadowTarget->BindRead(14);

        RenderCommand::SetDepthMask(true);
        RenderCommand::SetBlend(false);

        std::vector<const RenderCommandPacket*> opaquePointers;
        opaquePointers.reserve(m_Data->OpaqueQueue.size());
        for (const auto& packet : m_Data->OpaqueQueue)
            opaquePointers.push_back(&packet);

        // Group identical meshes/shaders/materials first, then sort by camera distance
        std::sort(opaquePointers.begin(), opaquePointers.end(), [](const RenderCommandPacket* a, const RenderCommandPacket* b)
            {
                if (a->SortKey != b->SortKey)
                    return a->SortKey < b->SortKey;

                if (a->Material != b->Material)
                    return a->Material < b->Material;

                return a->DistanceToCamera < b->DistanceToCamera;
            });

        // WP7 fix: RenderingTransparent was declared and read by FlushBatch but
        // never set, so u_IsTransparent stayed 0 and transparent draws leaked
        // their normals into the GTAO/SSR G-buffer.
        // WP10: snapshot texture-unit state at the exact moment the opaque pass
        // samples it (diagnoses stale-binding bugs; shown in the stats panel).
        RenderCommand::QueryTextureBindings(m_Data->DebugTextureBindings, 32);

        m_Data->RenderingTransparent = false;
        {
            RXN_GPU_SCOPE("Opaque Pass");
            ExecuteQueue(opaquePointers);
        }

        RenderCommand::SetDepthMask(false);
        RenderCommand::SetBlend(true);

        RenderCommand::SetBlendFunc(RendererAPI::BlendFactor::SrcAlpha, RendererAPI::BlendFactor::OneMinusSrcAlpha);

        std::vector<const RenderCommandPacket*> transparentPointers;
        transparentPointers.reserve(m_Data->TransparentQueue.size());

        for (const auto& packet : m_Data->TransparentQueue)
            transparentPointers.push_back(&packet);

        std::sort(transparentPointers.begin(), transparentPointers.end(), [](const RenderCommandPacket* a, const RenderCommandPacket* b)
            {
                if (a->SortKey != b->SortKey)
                    return a->SortKey < b->SortKey;

                if (a->Material != b->Material)
                    return a->Material < b->Material;

                if (glm::abs(a->DistanceToCamera - b->DistanceToCamera) < 0.001f)
                    return a->EntityID < b->EntityID;

                return a->DistanceToCamera > b->DistanceToCamera;
            });

        m_Data->RenderingTransparent = true;

        // WP8: hard-block G-buffer writes (SSR attachment 1, normal attachment 2)
        // during the transparent pass with per-attachment color masks. Unlike the
        // previous blend-based protection (o_Normal.a = 0), this cannot be broken
        // by external GL state changes -- which is what caused transparents to
        // leak their normals into GTAO in play mode but not in the editor.
        // WP11: only block SSR (attachment 1). Transparent objects must still write
        // their world-space normals (attachment 2) so GTAO does not collapse to 0
        // on glass/lamp-shade surfaces that scripts mark as transparent at runtime.
        RenderCommand::SetColorMaskIndexed(1, false, false, false, false);
        {
            RXN_GPU_SCOPE("Transparent Pass");
            ExecuteQueue(transparentPointers);
        }
        RenderCommand::SetColorMaskIndexed(1, true, true, true, true);
        m_Data->RenderingTransparent = false;

        RenderCommand::SetDepthMask(true);
        RenderCommand::SetBlend(false);

        if (!m_Data->LineVertices.empty())
        {
            m_Data->LineVBO->SetData(m_Data->LineVertices.data(), m_Data->LineVertices.size() * sizeof(LineVertex));

            m_Data->LineShader->Bind();
            m_Data->LineShader->SetMat4("u_ViewProjection", m_Data->ViewProjectionMatrix);

            // WP9: line.glsl writes a dummy o_Normal; keep it out of the G-buffer.
            RenderCommand::SetColorMaskIndexed(1, false, false, false, false);
            RenderCommand::SetColorMaskIndexed(2, false, false, false, false);
            RenderCommand::SetLineWidth(2.0f);
            RenderCommand::DrawLines(m_Data->LineVAO, m_Data->LineVertices.size());
            RenderCommand::SetColorMaskIndexed(1, true, true, true, true);
            RenderCommand::SetColorMaskIndexed(2, true, true, true, true);
        }
    }

    void Renderer::ExecuteQueue(const std::vector<const RenderCommandPacket*>& queue)
    {
        RXN_PROFILE_SCOPE();

        if (queue.empty()) 
            return;

        const RenderCommandPacket* batchStart = queue.front();

        InstanceData* currentBatchData = m_Data->BatchBuffer.data();
        uint32_t transformCount = 0;

        currentBatchData[transformCount].Transform = batchStart->Transform;
        currentBatchData[transformCount].EntityID = batchStart->EntityID;
        transformCount++;

        for (size_t i = 1; i < queue.size(); ++i)
        {
            const RenderCommandPacket* current = queue[i];

            bool isSameMesh = (current->Mesh == batchStart->Mesh && current->SubmeshIndex == batchStart->SubmeshIndex && current->LODIndex == batchStart->LODIndex);
            bool isSameMaterial = (current->Material == batchStart->Material);

            bool isSameVAO = false;
            if (isSameMesh)
            {
                isSameVAO = (current->ResolvedVAO == batchStart->ResolvedVAO);
            }

            if (isSameMesh && isSameMaterial && isSameVAO && transformCount < MaxInstances)
            {
                currentBatchData[transformCount].Transform = current->Transform;
                currentBatchData[transformCount].EntityID = current->EntityID;
                transformCount++;
            }
            else
            {
                FlushBatch(batchStart->Mesh, batchStart->SubmeshIndex, batchStart->Material, currentBatchData, transformCount, batchStart->LODIndex, batchStart->ResolvedVAO, batchStart->ResolvedSkeletalMesh);

                batchStart = current;
                transformCount = 0;
                currentBatchData[transformCount].Transform = current->Transform;
                currentBatchData[transformCount].EntityID = current->EntityID;
                transformCount++;
            }
        }

        if (transformCount > 0)
            FlushBatch(batchStart->Mesh, batchStart->SubmeshIndex, batchStart->Material, currentBatchData, transformCount, batchStart->LODIndex, batchStart->ResolvedVAO, batchStart->ResolvedSkeletalMesh);
    }

    void Renderer::FlushBatch(const Ref<StaticMesh>& mesh, uint32_t submeshIndex, const Ref<Material>& material, const InstanceData* instanceData, uint32_t count, uint32_t lodIndex, const Ref<VertexArray>& resolvedVAO, const Ref<SkeletalMesh>& resolvedSkeletalMesh)
    {
        RXN_PROFILE_SCOPE();

        if (count == 0)
            return;

        // Same null-material guard as Renderer::Submit (instanced path).
        if (!material || !material->GetShader())
        {
            static bool s_WarnedNullBatchMaterial = false;
            if (!s_WarnedNullBatchMaterial)
            {
                RXN_CORE_WARN("Renderer::FlushBatch: a batched submesh has no material/shader assigned; skipping {0} instances. Assign a material to this mesh slot.", count);
                s_WarnedNullBatchMaterial = true;
            }
            return;
        }

        uint32_t instanceOffset = m_Data->InstanceVertexBuffer->Push(instanceData, count * sizeof(InstanceData));
        material->Bind();

        Ref<Shader> shader = material->GetShader();
        if (m_Data->CurrentShaderID != shader->GetRendererID())
        {
            shader->Bind();
            m_Data->CurrentShaderID = shader->GetRendererID();
            shader->SetMat4("u_ViewProjection", m_Data->ViewProjectionMatrix);
            shader->SetMat4("u_InverseViewProjection", glm::inverse(m_Data->ViewProjectionMatrix));
            shader->SetMat4("u_PrevViewProjection", m_Data->PrevViewProjectionMatrix);
            shader->SetMat4("u_PrevInverseViewProjection", glm::inverse(m_Data->PrevViewProjectionMatrix));
            shader->SetFloat3("u_CameraPosition", m_Data->CameraPosition);
            shader->SetMat4("u_View", m_Data->ViewMatrix);
            shader->SetInt("u_ClusteredEnabled", m_Data->ClusteredLightingEnabled ? 1 : 0);
            shader->SetFloat("u_PointShadowResolution", (float)m_Data->PointShadowTarget->GetSize());

            shader->SetInt("u_AmbientVolumeCount", (int)m_Data->ActiveAmbientVolumes.size());
            for (size_t i = 0; i < m_Data->ActiveAmbientVolumes.size(); i++)
            {
                std::string prefix = "u_AmbientVolumes[" + std::to_string(i) + "].";
                shader->SetMat4(prefix + "InverseTransform", m_Data->ActiveAmbientVolumes[i].InverseTransform);
                shader->SetFloat3(prefix + "HalfExtents", m_Data->ActiveAmbientVolumes[i].HalfExtents);
                shader->SetFloat(prefix + "Intensity", m_Data->ActiveAmbientVolumes[i].Intensity);
                shader->SetFloat3(prefix + "TransitionMin", m_Data->ActiveAmbientVolumes[i].TransitionMin);
                shader->SetFloat3(prefix + "TransitionMax", m_Data->ActiveAmbientVolumes[i].TransitionMax);
            }

            shader->SetInt("u_ReflectionProbeCount", (int)m_Data->ActiveReflectionProbes.size());
            for (size_t i = 0; i < m_Data->ActiveReflectionProbes.size(); i++)
            {
                std::string prefix = "u_ReflectionProbes[" + std::to_string(i) + "].";
                shader->SetMat4(prefix + "Transform", m_Data->ActiveReflectionProbes[i].Transform);
                shader->SetMat4(prefix + "InverseTransform", m_Data->ActiveReflectionProbes[i].InverseTransform);
                shader->SetFloat3(prefix + "HalfExtents", m_Data->ActiveReflectionProbes[i].HalfExtents);
                shader->SetFloat(prefix + "BlendDistance", m_Data->ActiveReflectionProbes[i].BlendDistance);
                shader->SetFloat(prefix + "Intensity", m_Data->ActiveReflectionProbes[i].Intensity);
            }
        }

        Ref<SkeletalMesh> skeletalMesh = resolvedSkeletalMesh;
        // GTAO fix: tell the shader whether this draw is transparent so it can avoid
        // clobbering the opaque normal/SSR G-buffer (must run every batch, not just on shader change).
        shader->SetInt("u_IsTransparent", m_Data->RenderingTransparent ? 1 : 0);

        Ref<VertexArray> targetVAO = resolvedVAO ? resolvedVAO : mesh->GetVertexArray();
        targetVAO->Bind();

        uint32_t indexCount = 0;
        uint32_t baseIndex = 0;
        if (skeletalMesh && submeshIndex < skeletalMesh->GetSubmeshes().size())
        {
            const auto& submesh = skeletalMesh->GetSubmeshes()[submeshIndex];
            uint32_t actualLod = lodIndex < submesh.LODs.size() ? lodIndex : 0;
            const auto& lod = submesh.LODs[actualLod];
            indexCount = lod.IndexCount;
            baseIndex = lod.BaseIndex;
        }
        else
        {
            const auto& submesh = mesh->GetSubmeshes()[submeshIndex];
            uint32_t actualLod = lodIndex < submesh.LODs.size() ? lodIndex : 0;
            const auto& lod = submesh.LODs[actualLod];
            indexCount = lod.IndexCount;
            baseIndex = lod.BaseIndex;
        }
        RenderCommand::DrawIndexedInstancedStream(targetVAO, m_Data->InstanceVertexBuffer, instanceOffset, count, indexCount, baseIndex);

        m_Data->Stats.DrawCalls++;
        m_Data->Stats.Instances += count;
        m_Data->Stats.TotalIndices += indexCount * count;
    }

    void Renderer::FlushShadows()
    {
        RXN_PROFILE_SCOPE();

        if (!m_Data->DirLightCastsShadows)
        {
            for (uint32_t cascade = 0; cascade < 4; cascade++)
            {
                m_Data->ShadowData.ShadowTarget->BindWriteLayer(cascade);
            }
            m_Data->ShadowData.ForceAllCascades = true; // WP18: layers were cleared; refresh all when re-enabled
            return;
        }

        m_Data->ShadowData.ShadowShader->Bind();

        for (uint32_t cascade = 0; cascade < 4; cascade++)
        {
            // WP18: staggered cascade — keep last frame's depth. Skipping
            // BEFORE BindWriteLayer matters because BindWriteLayer clears the
            // layer; a skipped cascade must be neither cleared nor drawn.
            if (!m_Data->ShadowData.CascadeNeedsRender[cascade])
                continue;

            m_Data->ShadowData.ShadowTarget->BindWriteLayer(cascade);

            static const char* s_CascadeScopeNames[4] = { "CSM Cascade 0", "CSM Cascade 1", "CSM Cascade 2", "CSM Cascade 3" };
            RXN_GPU_SCOPE(s_CascadeScopeNames[cascade]);

            const glm::mat4& M = m_Data->ShadowData.BufferLocal.LightSpaceMatrices[cascade];
            m_Data->ShadowData.ShadowShader->SetMat4("u_LightSpaceMatrix", M);

            if (m_Data->ShadowQueue.empty())
                continue;

            InstanceData* batchData = m_Data->BatchBuffer.data();
            uint32_t transformCount = 0;
            const RenderCommandPacket* batchStart = nullptr;

            for (const auto& packet : m_Data->ShadowQueue)
            {
                float minRadius = 0.0f;

                if (cascade == 1)
                    minRadius = 0.15f;
                else if (cascade == 2)
                    minRadius = 0.50f;
                else if (cascade == 3) 
                    minRadius = 1.50f;

                if (packet.BoundingRadius < minRadius)
                    continue;

                glm::vec4 centerNDC = M * glm::vec4(packet.BoundingCenter, 1.0f);
                float rX = packet.BoundingRadius * glm::length(glm::vec3(M[0][0], M[1][0], M[2][0]));
                float rY = packet.BoundingRadius * glm::length(glm::vec3(M[0][1], M[1][1], M[2][1]));
                float rZ = packet.BoundingRadius * glm::length(glm::vec3(M[0][2], M[1][2], M[2][2]));

                if (!(glm::abs(centerNDC.x) <= 1.0f + rX && glm::abs(centerNDC.y) <= 1.0f + rY &&
                    centerNDC.z >= -1.0f - rZ && centerNDC.z <= 1.0f + rZ))
                {
                    continue;
                }

                if (!batchStart)
                {
                    batchStart = &packet;
                    batchData[transformCount].Transform = packet.Transform;
                    batchData[transformCount].EntityID = packet.EntityID;
                    transformCount++;
                    continue;
                }

                bool isSameMesh = (packet.Mesh == batchStart->Mesh && packet.SubmeshIndex == batchStart->SubmeshIndex && packet.LODIndex == batchStart->LODIndex);

                bool isSameVAO = false;
                if (isSameMesh)
                {
                    isSameVAO = (packet.ResolvedVAO == batchStart->ResolvedVAO);
                }

                if (isSameMesh && isSameVAO && transformCount < MaxInstances)
                {
                    batchData[transformCount].Transform = packet.Transform;
                    batchData[transformCount].EntityID = packet.EntityID;
                    transformCount++;
                }
                else
                {
                    uint32_t instanceOffset = m_Data->InstanceVertexBuffer->Push(batchData, transformCount * sizeof(InstanceData));
                    Ref<SkeletalMesh> skeletalMesh = batchStart->ResolvedSkeletalMesh;
                    Ref<VertexArray> targetVAO = batchStart->ResolvedVAO ? batchStart->ResolvedVAO : batchStart->Mesh->GetVertexArray();
                    targetVAO->Bind();
                    uint32_t indexCount = 0;
                    uint32_t baseIndex = 0;
                    if (skeletalMesh && batchStart->SubmeshIndex < skeletalMesh->GetSubmeshes().size())
                    {
                        const auto& submesh = skeletalMesh->GetSubmeshes()[batchStart->SubmeshIndex];
                        uint32_t actualLod = batchStart->LODIndex < submesh.LODs.size() ? batchStart->LODIndex : 0;
                        const auto& lod = submesh.LODs[actualLod];
                        indexCount = lod.IndexCount;
                        baseIndex = lod.BaseIndex;
                    }
                    else
                    {
                        const auto& submesh = batchStart->Mesh->GetSubmeshes()[batchStart->SubmeshIndex];
                        uint32_t actualLod = batchStart->LODIndex < submesh.LODs.size() ? batchStart->LODIndex : 0;
                        const auto& lod = submesh.LODs[actualLod];
                        indexCount = lod.IndexCount;
                        baseIndex = lod.BaseIndex;
                    }
                    RenderCommand::DrawIndexedInstancedStream(targetVAO, m_Data->InstanceVertexBuffer, instanceOffset, transformCount, indexCount, baseIndex);

                    if (cascade == 0)
                    {
                        m_Data->Stats.DrawCalls++;
                        m_Data->Stats.Instances += transformCount;
                        m_Data->Stats.TotalIndices += indexCount * transformCount;
                    }

                    batchStart = &packet;
                    transformCount = 0;
                    batchData[transformCount].Transform = packet.Transform;
                    batchData[transformCount].EntityID = packet.EntityID;
                    transformCount++;
                }
            }

            if (transformCount > 0 && batchStart)
            {
                uint32_t instanceOffset = m_Data->InstanceVertexBuffer->Push(batchData, transformCount * sizeof(InstanceData));
                Ref<SkeletalMesh> skeletalMesh = batchStart->ResolvedSkeletalMesh;
                Ref<VertexArray> targetVAO = batchStart->ResolvedVAO ? batchStart->ResolvedVAO : batchStart->Mesh->GetVertexArray();
                targetVAO->Bind();
                uint32_t indexCount = 0;
                uint32_t baseIndex = 0;
                if (skeletalMesh && batchStart->SubmeshIndex < skeletalMesh->GetSubmeshes().size())
                {
                    const auto& submesh = skeletalMesh->GetSubmeshes()[batchStart->SubmeshIndex];
                    uint32_t actualLod = batchStart->LODIndex < submesh.LODs.size() ? batchStart->LODIndex : 0;
                    const auto& lod = submesh.LODs[actualLod];
                    indexCount = lod.IndexCount;
                    baseIndex = lod.BaseIndex;
                }
                else
                {
                    const auto& submesh = batchStart->Mesh->GetSubmeshes()[batchStart->SubmeshIndex];
                    uint32_t actualLod = batchStart->LODIndex < submesh.LODs.size() ? batchStart->LODIndex : 0;
                    const auto& lod = submesh.LODs[actualLod];
                    indexCount = lod.IndexCount;
                    baseIndex = lod.BaseIndex;
                }
                RenderCommand::DrawIndexedInstancedStream(targetVAO, m_Data->InstanceVertexBuffer, instanceOffset, transformCount, indexCount, baseIndex);

                if (cascade == 0)
                {
                    m_Data->Stats.DrawCalls++;
                    m_Data->Stats.Instances += transformCount;
                    m_Data->Stats.TotalIndices += indexCount * transformCount;
                }
            }
        }
    }

    void Renderer::FlushSpotShadows()
    {
        for (uint32_t i = 0; i < m_Data->LightBufferLocal.SpotLightCount; i++)
        {
            if (m_Data->SpotLightsGPU[i].ExtraData.x < 0.0f)
                continue;

            uint32_t layer = (uint32_t)m_Data->SpotLightsGPU[i].ExtraData.x;

            glm::vec3 lightPos = m_Data->SpotLightsGPU[i].Position;
            float lightRadius = m_Data->SpotLightsGPU[i].Direction.w;

            int entityID = m_Data->SpotLightEntityIDs[i];
            bool hasCache = false;
            bool isCacheValid = false;
            Entity entity;

            if (m_Data->ActiveScene && entityID != -1)
            {
                entity = Entity((entt::entity)entityID, m_Data->ActiveScene);
                if (entity && entity.HasComponent<SpotLightComponent>())
                {
                    hasCache = true;
                    auto& slc = entity.GetComponent<SpotLightComponent>();

                    if (glm::distance(slc.LastCachedPosition, lightPos) > 0.01f)
                        slc.IsShadowCacheValid = false;

                    // WP20: a flashlight mostly ROTATES, which the position
                    // check alone cannot see. The light-space matrix encodes
                    // position, direction, cone angle and range, so compare
                    // the whole thing: if it changed, the cached depth no
                    // longer matches the matrix the lighting pass samples
                    // with, and the shadow would be frozen/garbage.
                    {
                        const glm::mat4& cur = m_Data->SpotLightsGPU[i].LightSpaceMatrix;
                        const glm::mat4& old = slc.LastCachedMatrix;
                        float maxDiff = 0.0f;
                        for (int c = 0; c < 4; ++c)
                            for (int r = 0; r < 4; ++r)
                                maxDiff = glm::max(maxDiff, glm::abs(cur[c][r] - old[c][r]));
                        if (maxDiff > 1e-4f)
                            slc.IsShadowCacheValid = false;
                    }

                    if (slc.LastShadowLayer != (int)layer)
                    {
                        slc.IsShadowCacheValid = false;
                        slc.LastShadowLayer = (int)layer;
                    }

                    isCacheValid = slc.IsShadowCacheValid;
                }
            }

            bool isVolumeDirty = false;
            uint32_t dynamicOverlapCount = 0;
            uint32_t totalOverlapCount = 0;

            for (const auto& packet : m_Data->ShadowQueue)
            {
                float distance = glm::distance(packet.BoundingCenter, lightPos);
                if (distance <= lightRadius + packet.BoundingRadius)
                {
                    totalOverlapCount++;
                    if (packet.IsDynamic)
                    {
                        isVolumeDirty = true;
                        dynamicOverlapCount++;
                    }
                }
            }

            if (hasCache && isCacheValid && !isVolumeDirty)
                continue;

            m_Data->SpotShadowTarget->BindWriteLayer(layer);
            DrawShadowBatch(m_Data, m_Data->ShadowData.ShadowShader, m_Data->SpotLightsGPU[i].LightSpaceMatrix, false, lightPos, lightRadius);

            if (hasCache)
            {
                auto& slc = entity.GetComponent<SpotLightComponent>();
                slc.LastCachedPosition = lightPos;
                slc.LastCachedMatrix = m_Data->SpotLightsGPU[i].LightSpaceMatrix; // WP20
                slc.IsShadowCacheValid = !isVolumeDirty;
            }
        }
    }

    void Renderer::FlushPointShadows()
    {
        for (uint32_t i = 0; i < m_Data->LightBufferLocal.PointLightCount; i++)
        {
            if (m_Data->PointLightsGPU[i].ExtraData.x < 0.0f)
                continue;

            uint32_t layer = (uint32_t)m_Data->PointLightsGPU[i].ExtraData.x;

            glm::vec3 lightPos = m_Data->PointLightsGPU[i].Position;
            float farPlane = m_Data->PointLightsGPU[i].Color.w; // Radius

            int entityID = m_Data->PointLightEntityIDs[i];
            bool hasCache = false;
            bool isCacheValid = false;
            Entity entity;

            if (m_Data->ActiveScene && entityID != -1)
            {
                entity = Entity((entt::entity)entityID, m_Data->ActiveScene);
                if (entity && entity.HasComponent<PointLightComponent>())
                {
                    hasCache = true;
                    auto& plc = entity.GetComponent<PointLightComponent>();

                    if (glm::distance(plc.LastCachedPosition, lightPos) > 0.01f)
                        plc.IsShadowCacheValid = false;

                    // WP20: the radius is the far plane of all six cube-face
                    // projections. If it changes (script-driven torch flicker,
                    // editor tweak), the cached depth no longer matches the
                    // projection the lighting pass unprojects with.
                    if (glm::abs(plc.LastCachedRadius - farPlane) > 1e-3f)
                        plc.IsShadowCacheValid = false;

                    if (plc.LastShadowLayer != (int)layer)
                    {
                        plc.IsShadowCacheValid = false;
                        plc.LastShadowLayer = (int)layer;
                    }

                    isCacheValid = plc.IsShadowCacheValid;
                }
            }

            bool isVolumeDirty = false;
            uint32_t dynamicOverlapCount = 0;
            uint32_t totalOverlapCount = 0;

            for (const auto& packet : m_Data->ShadowQueue)
            {
                float distance = glm::distance(packet.BoundingCenter, lightPos);
                if (distance <= farPlane + packet.BoundingRadius)
                {
                    totalOverlapCount++;
                    if (packet.IsDynamic)
                    {
                        isVolumeDirty = true;
                        dynamicOverlapCount++;
                    }
                }
            }

            if (hasCache && isCacheValid && !isVolumeDirty)
                continue;

            glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, farPlane);
            glm::mat4 shadowTransforms[] = {
                shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(1, 0, 0), glm::vec3(0,-1, 0)),
                shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
                shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)),
                shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0,-1, 0), glm::vec3(0, 0,-1)),
                shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0, 0, 1), glm::vec3(0,-1, 0)),
                shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0, 0,-1), glm::vec3(0,-1, 0))
            };

            for (uint32_t face = 0; face < 6; face++)
            {
                m_Data->PointShadowTarget->BindWriteLayer(layer, face);
                DrawShadowBatch(m_Data, m_Data->PointShadowShader, shadowTransforms[face], true, lightPos, farPlane);
            }

            if (hasCache)
            {
                auto& plc = entity.GetComponent<PointLightComponent>();
                plc.LastCachedPosition = lightPos;
                plc.LastCachedRadius = farPlane; // WP20
                plc.IsShadowCacheValid = !isVolumeDirty;
            }
        }
    }

    void Renderer::ExecutePickingPass(const Ref<Shader>& pickingShader)
    {
        RXN_PROFILE_SCOPE();

        auto drawQueue = [&](const std::vector<RenderCommandPacket>& queue)
            {
                if (queue.empty())
                    return;

                auto batchStart = queue.begin();
                InstanceData* batchData = m_Data->BatchBuffer.data();
                uint32_t transformCount = 0;

                batchData[transformCount].Transform = batchStart->Transform;
                batchData[transformCount].EntityID = batchStart->EntityID;
                transformCount++;

                for (auto it = queue.begin() + 1; it != queue.end(); ++it)
                {
                    bool isSameMesh = (it->Mesh == batchStart->Mesh && it->SubmeshIndex == batchStart->SubmeshIndex);

                    if (isSameMesh && transformCount < MaxInstances)
                    {
                        batchData[transformCount].Transform = it->Transform;
                        batchData[transformCount].EntityID = it->EntityID;
                        transformCount++;
                    }
                    else
                    {
                        uint32_t instanceOffset = m_Data->InstanceVertexBuffer->Push(batchData, transformCount * sizeof(InstanceData));

                        batchStart->Mesh->GetVertexArray()->Bind();

                        const auto& submesh = batchStart->Mesh->GetSubmeshes()[batchStart->SubmeshIndex];
                        RenderCommand::DrawIndexedInstancedStream(batchStart->Mesh->GetVertexArray(), m_Data->InstanceVertexBuffer, instanceOffset, transformCount, submesh.IndexCount, submesh.BaseIndex);

                        batchStart = it;
                        transformCount = 0;
                        batchData[transformCount].Transform = it->Transform;
                        batchData[transformCount].EntityID = it->EntityID;
                        transformCount++;
                    }
                }

                if (transformCount > 0)
                {
                    uint32_t instanceOffset = m_Data->InstanceVertexBuffer->Push(batchData, transformCount * sizeof(InstanceData));

                    batchStart->Mesh->GetVertexArray()->Bind();

                    const auto& submesh = batchStart->Mesh->GetSubmeshes()[batchStart->SubmeshIndex];
                    RenderCommand::DrawIndexedInstancedStream(batchStart->Mesh->GetVertexArray(), m_Data->InstanceVertexBuffer, instanceOffset, transformCount, submesh.IndexCount, submesh.BaseIndex);
                }
            };

        drawQueue(m_Data->OpaqueQueue);
        drawQueue(m_Data->TransparentQueue);
    }

    bool Renderer::IsSphereVisibleToShadows(const glm::vec3& center, float radius)
    {
        for (int i = 0; i < 4; i++)
        {
            const glm::mat4& M = m_Data->ShadowData.BufferLocal.LightSpaceMatrices[i];
            glm::vec4 centerNDC = M * glm::vec4(center, 1.0f);

            float rX = radius * glm::length(glm::vec3(M[0][0], M[1][0], M[2][0]));
            float rY = radius * glm::length(glm::vec3(M[0][1], M[1][1], M[2][1]));
            float rZ = radius * glm::length(glm::vec3(M[0][2], M[1][2], M[2][2]));

            if (glm::abs(centerNDC.x) <= 1.0f + rX && glm::abs(centerNDC.y) <= 1.0f + rY &&
                centerNDC.z >= -1.0f - rZ && centerNDC.z <= 1.0f + rZ)
            {
                return true;
            }
        }
        return false;
    }

    void Renderer::SubmitShadowCaster(const Ref<StaticMesh>& mesh, uint32_t submeshIndex, const glm::mat4& transform, int entityID, const glm::vec3& boundingCenter, float boundingRadius, bool isDynamic, uint32_t lodIndex)
    {
        const auto& submeshes = mesh->GetSubmeshes();
        if (submeshIndex >= submeshes.size())
            return;

        // WP5: shadow maps never need full-resolution geometry. Bias shadow
        // casters to a coarser LOD: the cascades' vertex cost drops with the
        // triangle count, and PCSS filtering blurs the shadow far more than
        // the geometric simplification changes it.
        constexpr uint32_t kShadowLODBias = 2;
        uint32_t shadowLod = lodIndex;
        uint32_t lodCount = (uint32_t)submeshes[submeshIndex].LODs.size();
        if (lodCount > 0)
            shadowLod = (lodIndex + kShadowLODBias < lodCount) ? (lodIndex + kShadowLODBias) : (lodCount - 1);

        RenderCommandPacket packet;
        packet.Mesh = mesh;
        packet.SubmeshIndex = submeshIndex;
        packet.Transform = transform;
        packet.EntityID = entityID;
        packet.LODIndex = shadowLod;

        packet.BoundingCenter = boundingCenter;
        packet.BoundingRadius = boundingRadius;
        packet.IsDynamic = isDynamic;

        Ref<SkeletalMesh> skeletalMesh = nullptr;
        packet.ResolvedVAO = GetActiveVAO(mesh, entityID, m_Data->ActiveScene, skeletalMesh);
        packet.ResolvedSkeletalMesh = skeletalMesh;

        m_Data->ShadowQueue.push_back(packet);
    }

    void Renderer::SubmitShadowImpostorQuad(Entity entity)
    {
        //TODO
    }

    void Renderer::DrawShadowBatch(RendererData* data, Ref<Shader> shader, const glm::mat4& matrix, bool isOmni, const glm::vec3& lightPos, float farPlane)
    {
        shader->Bind();
        shader->SetMat4("u_LightSpaceMatrix", matrix);
        if (isOmni)
        {
            shader->SetFloat3("u_LightPos", lightPos);
            shader->SetFloat("u_FarPlane", farPlane);
        }

        if (data->ShadowQueue.empty())
            return;

        glm::vec4 planes[6];
        for (int i = 0; i < 4; ++i)
            planes[0][i] = matrix[i][3] + matrix[i][0]; // Left
        for (int i = 0; i < 4; ++i)
            planes[1][i] = matrix[i][3] - matrix[i][0]; // Right
        for (int i = 0; i < 4; ++i)
            planes[2][i] = matrix[i][3] + matrix[i][1]; // Bottom
        for (int i = 0; i < 4; ++i)
            planes[3][i] = matrix[i][3] - matrix[i][1]; // Top
        for (int i = 0; i < 4; ++i)
            planes[4][i] = matrix[i][3] + matrix[i][2]; // Near
        for (int i = 0; i < 4; ++i)
            planes[5][i] = matrix[i][3] - matrix[i][2]; // Far

        for (int i = 0; i < 6; ++i)
        {
            float length = glm::length(glm::vec3(planes[i]));
            planes[i] /= length;
        }

        InstanceData* batchData = data->BatchBuffer.data();
        uint32_t transformCount = 0;
        const RenderCommandPacket* batchStart = nullptr;

        for (const auto& packet : data->ShadowQueue)
        {
            float minRadius = isOmni ? 0.15f : 0.10f; // 15cm for points, 10cm for spots
            if (packet.BoundingRadius < minRadius)
                continue;

            float dist = glm::distance(packet.BoundingCenter, lightPos);
            if (dist > farPlane + packet.BoundingRadius)
                continue;

            bool visible = true;
            for (int i = 0; i < 6; ++i)
            {
                if (glm::dot(glm::vec3(planes[i]), packet.BoundingCenter) + planes[i].w < -packet.BoundingRadius)
                {
                    visible = false;
                    break;
                }
            }

            if (!visible)
                continue;

            if (!batchStart)
            {
                batchStart = &packet;
                batchData[transformCount].Transform = packet.Transform;
                transformCount++;
                continue;
            }

            bool isSameMesh = (packet.Mesh == batchStart->Mesh && packet.SubmeshIndex == batchStart->SubmeshIndex && packet.LODIndex == batchStart->LODIndex);
            bool isSameVAO = false;
            if (isSameMesh)
            {
                isSameVAO = (packet.ResolvedVAO == batchStart->ResolvedVAO);
            }

            if (isSameMesh && isSameVAO && transformCount < MaxInstances)
            {
                batchData[transformCount].Transform = packet.Transform;
                transformCount++;
            }
            else
            {
                uint32_t instanceOffset = data->InstanceVertexBuffer->Push(batchData, transformCount * sizeof(InstanceData));
                Ref<SkeletalMesh> skeletalMesh = batchStart->ResolvedSkeletalMesh;
                Ref<VertexArray> targetVAO = batchStart->ResolvedVAO ? batchStart->ResolvedVAO : batchStart->Mesh->GetVertexArray();
                targetVAO->Bind();
                uint32_t indexCount = 0;
                uint32_t baseIndex = 0;
                if (skeletalMesh && batchStart->SubmeshIndex < skeletalMesh->GetSubmeshes().size())
                {
                    const auto& submesh = skeletalMesh->GetSubmeshes()[batchStart->SubmeshIndex];
                    uint32_t actualLod = batchStart->LODIndex < submesh.LODs.size() ? batchStart->LODIndex : 0;
                    const auto& lod = submesh.LODs[actualLod];
                    indexCount = lod.IndexCount;
                    baseIndex = lod.BaseIndex;
                }
                else
                {
                    const auto& submesh = batchStart->Mesh->GetSubmeshes()[batchStart->SubmeshIndex];
                    uint32_t actualLod = batchStart->LODIndex < submesh.LODs.size() ? batchStart->LODIndex : 0;
                    const auto& lod = submesh.LODs[actualLod];
                    indexCount = lod.IndexCount;
                    baseIndex = lod.BaseIndex;
                }
                RenderCommand::DrawIndexedInstancedStream(targetVAO, data->InstanceVertexBuffer, instanceOffset, transformCount, indexCount, baseIndex);

                data->Stats.DrawCalls++;
                data->Stats.Instances += transformCount;
                data->Stats.TotalIndices += indexCount * transformCount;

                batchStart = &packet;
                transformCount = 0;
                batchData[transformCount].Transform = packet.Transform;
                transformCount++;
            }
        }

        if (transformCount > 0 && batchStart)
        {
            uint32_t instanceOffset = data->InstanceVertexBuffer->Push(batchData, transformCount * sizeof(InstanceData));
            Ref<SkeletalMesh> skeletalMesh = batchStart->ResolvedSkeletalMesh;
            Ref<VertexArray> targetVAO = batchStart->ResolvedVAO ? batchStart->ResolvedVAO : batchStart->Mesh->GetVertexArray();
            targetVAO->Bind();
            uint32_t indexCount = 0;
            uint32_t baseIndex = 0;
            if (skeletalMesh && batchStart->SubmeshIndex < skeletalMesh->GetSubmeshes().size())
            {
                const auto& submesh = skeletalMesh->GetSubmeshes()[batchStart->SubmeshIndex];
                uint32_t actualLod = batchStart->LODIndex < submesh.LODs.size() ? batchStart->LODIndex : 0;
                const auto& lod = submesh.LODs[actualLod];
                indexCount = lod.IndexCount;
                baseIndex = lod.BaseIndex;
            }
            else
            {
                const auto& submesh = batchStart->Mesh->GetSubmeshes()[batchStart->SubmeshIndex];
                uint32_t actualLod = batchStart->LODIndex < submesh.LODs.size() ? batchStart->LODIndex : 0;
                const auto& lod = submesh.LODs[actualLod];
                indexCount = lod.IndexCount;
                baseIndex = lod.BaseIndex;
            }
            RenderCommand::DrawIndexedInstancedStream(targetVAO, data->InstanceVertexBuffer, instanceOffset, transformCount, indexCount, baseIndex);

            data->Stats.DrawCalls++;
            data->Stats.Instances += transformCount;
            data->Stats.TotalIndices += indexCount * transformCount;
        }
    }
}