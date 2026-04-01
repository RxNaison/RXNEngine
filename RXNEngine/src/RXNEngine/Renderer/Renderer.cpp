#include "rxnpch.h"
#include "Renderer.h"
#include "RXNEngine/Math/Math.h"
#include "RXNEngine/Math/Frustum.h"
#include "RXNEngine/Renderer/GraphicsAPI/UniformBuffer.h"
#include "RenderCommand.h"
#include "ShadowMap.h"

#include <algorithm>
#include <array>

namespace RXNEngine {

    struct LightDataGPU
    {
        glm::vec4 DirLightDirection;
        glm::vec4 DirLightColor;

        uint32_t PointLightCount;
        uint32_t Padding[3];      // align to 16 bytes

        struct PointLightGPU
        {
            glm::vec4 Position;  // w = Intensity
            glm::vec4 Color;     // w = Radius
            glm::vec4 Falloff;   // x = Falloff, yzw = Padding
        } PointLights[100];
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
        } BufferLocal;

        std::vector<float> CascadeSplits;
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
        glm::mat4 ViewProjectionMatrix;
        glm::mat4 ViewMatrix;
        glm::vec3 CameraPosition;
        glm::vec3 CameraForward;
        Frustum CameraFrustum;
        float CameraFOV = 45.0f;

        uint32_t CurrentShaderID = 0;
        uint32_t CurrentVertexArrayID = 0;

        std::array<uint32_t, 32> TextureSlots{ 0 };

        Ref<VertexBuffer> InstanceVertexBuffer;
        glm::mat4* InstanceBufferBase = nullptr;
        uint32_t InstanceCount = 0;

        Ref<UniformBuffer> LightUniformBuffer;
        LightDataGPU LightBufferLocal;

        Ref<VertexArray> SkyboxVAO;
        Ref<Shader> SkyboxShader;

        Ref<Cubemap> SceneEnvironment;
        ShadowData ShadowData;

        Ref<VertexArray> LineVAO;
        Ref<VertexBuffer> LineVBO;
        Ref<Shader> LineShader;

        std::vector<LineVertex> LineVertices;
        const uint32_t MaxLineVertices = 100000;

        RendererStatistics Stats;
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
        OPTICK_EVENT();

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
        OPTICK_EVENT();

        float aspect = (float)m_Data->CurrentRenderTarget->GetSpecification().Width / (float)m_Data->CurrentRenderTarget->GetSpecification().Height;
        float fov = m_Data->CameraFOV < 0.01f ? glm::radians(45.0f) : m_Data->CameraFOV;
        float nearPlane = 0.1f;

        for (size_t i = 0; i < 4; ++i)
        {
            float pNear = (i == 0) ? nearPlane : m_Data->ShadowData.CascadeSplits[i - 1];
            float pFar = m_Data->ShadowData.CascadeSplits[i];

            glm::mat4 proj = glm::perspective(fov, aspect, pNear, pFar);
            auto corners = GetFrustumCornersWorldSpace(proj, cameraView);

            glm::vec3 center = glm::vec3(0, 0, 0);
            for (const auto& v : corners) center += glm::vec3(v);
            center /= corners.size();

            const auto lightView = glm::lookAt(center - glm::normalize(lightDir), center, glm::vec3(0.0f, 1.0f, 0.0f));

            float minX = std::numeric_limits<float>::max(); float maxX = std::numeric_limits<float>::lowest();
            float minY = std::numeric_limits<float>::max(); float maxY = std::numeric_limits<float>::lowest();
            float minZ = std::numeric_limits<float>::max(); float maxZ = std::numeric_limits<float>::lowest();

            for (const auto& v : corners)
            {
                const auto trf = lightView * v;
                minX = std::min(minX, trf.x); maxX = std::max(maxX, trf.x);
                minY = std::min(minY, trf.y); maxY = std::max(maxY, trf.y);
                minZ = std::min(minZ, trf.z); maxZ = std::max(maxZ, trf.z);
            }

            float zMult = 10.0f;
            if (minZ < 0) minZ *= zMult; else minZ /= zMult;
            if (maxZ < 0) maxZ /= zMult; else maxZ *= zMult;

            const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);

            m_Data->ShadowData.BufferLocal.LightSpaceMatrices[i] = lightProjection * lightView;
            m_Data->ShadowData.BufferLocal.CascadePlaneDistances[i].x = pFar;
        }

        m_Data->ShadowData.ShadowUniformBuffer->SetData(&m_Data->ShadowData.BufferLocal, sizeof(ShadowData::ShadowDataGPU));
    }

    void Renderer::Init()
    {
        m_Data = new RendererData();

        RenderCommand::Init();
        m_Data->OpaqueQueue.reserve(1000);
        m_Data->InstanceVertexBuffer = VertexBuffer::Create(MaxInstances * sizeof(InstanceData));
        m_Data->InstanceVertexBuffer->SetLayout({
            { ShaderDataType::Float4, "a_ModelMatrixCol0", false, true },
            { ShaderDataType::Float4, "a_ModelMatrixCol1", false, true },
            { ShaderDataType::Float4, "a_ModelMatrixCol2", false, true },
            { ShaderDataType::Float4, "a_ModelMatrixCol3", false, true },
            { ShaderDataType::Int,    "a_EntityID",        false, true }
            });
        m_Data->LightUniformBuffer = UniformBuffer::Create(sizeof(LightDataGPU), 1);

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

        m_Data->ShadowData.ShadowTarget = ShadowMap::Create(4096);
        m_Data->ShadowData.ShadowTarget->Init(4096);

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
        m_Data->OpaqueQueue.clear();
        delete m_Data;
        m_Data = nullptr;
    }

    void Renderer::OnWindowResize(uint32_t width, uint32_t height)
    {
        RenderCommand::SetViewport(0, 0, width, height);
    }

    void Renderer::BeginScene(const EditorCamera& camera, const LightEnvironment& lights,
        const Ref<Cubemap>& environment, const Ref<RenderTarget>& renderTarget)
    {
        OPTICK_EVENT();
        PrepareScene(camera.GetViewProjection(), camera.GetViewMatrix(), camera.GetPosition(), camera.GetFOV(), lights, environment, renderTarget);
    }

    void Renderer::BeginScene(const Camera& camera, const glm::mat4& transform, const LightEnvironment& lights,
        const Ref<Cubemap>& environment, const Ref<RenderTarget>& renderTarget)
    {
        OPTICK_EVENT();

        glm::mat4 viewProj = camera.GetProjection() * glm::inverse(transform);
        glm::mat4 view = glm::inverse(transform);
        glm::vec3 cameraPos = glm::vec3(transform[3]);
        float fov = 2.0f * glm::atan(1.0f / camera.GetProjection()[1][1]);

        PrepareScene(viewProj, view, cameraPos, fov, lights, environment, renderTarget);
    }

    void Renderer::PrepareScene(
        const glm::mat4& viewProjection,
        const glm::mat4& viewMatrix,
        const glm::vec3& cameraPosition,
        float cameraFOV,
        const LightEnvironment& lights,
        const Ref<Cubemap>& environment,
        const Ref<RenderTarget>& renderTarget)
    {
        OPTICK_EVENT();

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
        m_Data->ViewMatrix = viewMatrix;
        m_Data->CameraPosition = cameraPosition;
        m_Data->CameraForward = -glm::normalize(glm::vec3(glm::inverse(viewMatrix)[2]));
        m_Data->CameraFrustum.Define(viewProjection);

        m_Data->LightBufferLocal.DirLightDirection = glm::vec4(lights.DirLight.Direction, lights.DirLight.Intensity);
        m_Data->LightBufferLocal.DirLightColor = glm::vec4(lights.DirLight.Color, 0.0f);
        m_Data->LightBufferLocal.PointLightCount = std::min((uint32_t)lights.PointLights.size(), 100u);

        for (uint32_t i = 0; i < m_Data->LightBufferLocal.PointLightCount; i++)
        {
            const auto& light = lights.PointLights[i];
            m_Data->LightBufferLocal.PointLights[i].Position = glm::vec4(light.Position, light.Intensity);
            m_Data->LightBufferLocal.PointLights[i].Color = glm::vec4(light.Color, light.Radius);
            m_Data->LightBufferLocal.PointLights[i].Falloff = glm::vec4(light.Falloff, 0.0f, 0.0f, 0.0f);
        }

        m_Data->LightUniformBuffer->SetData(&m_Data->LightBufferLocal, sizeof(LightDataGPU));

        m_Data->OpaqueQueue.clear();
        m_Data->TransparentQueue.clear();
        m_Data->ShadowQueue.clear();

        m_Data->CurrentShaderID = 0;
        m_Data->CurrentVertexArrayID = 0;
        std::fill(m_Data->TextureSlots.begin(), m_Data->TextureSlots.end(), 0);
        m_Data->LineVertices.clear();

        CalculateShadowMapMatrices(m_Data->ViewMatrix, m_Data->LightBufferLocal.DirLightDirection);
    }

    void Renderer::Submit(const Ref<StaticMesh>& mesh, uint32_t submeshIndex, const Ref<Material>& material, const glm::mat4& transform, int entityID)
    {
        OPTICK_EVENT();

        const auto& submeshes = mesh->GetSubmeshes();
        if (submeshIndex >= submeshes.size()) return;

        RenderCommandPacket packet;
        packet.Mesh = mesh;
        packet.SubmeshIndex = submeshIndex;
        packet.Material = material;
        packet.Transform = transform;
        packet.EntityID = entityID;

        glm::vec3 position = glm::vec3(transform[3]);
        glm::vec3 directionToPosition = position - m_Data->CameraPosition;
        packet.DistanceToCamera = glm::dot(m_Data->CameraForward, directionToPosition);

        uint64_t shaderID = material->GetShader()->GetRendererID();
        uint64_t vaoID = mesh->GetVertexArray()->GetRendererID();

        packet.SortKey = (shaderID << 32) | ((vaoID & 0xFFFF) << 16) | (submeshIndex & 0xFFFF);

        if (material->IsTransparent())
            m_Data->TransparentQueue.push_back(packet);
        else
            m_Data->OpaqueQueue.push_back(packet);
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
        OPTICK_EVENT();

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
        OPTICK_EVENT();

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
        OPTICK_EVENT();

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

    void Renderer::DrawEntityOutline(const Ref<StaticMesh>& mesh, uint32_t submeshIndex, const glm::mat4& transform, const Ref<Shader>& outlineShader)
    {
        InstanceData data;
        data.Transform = transform;
        data.EntityID = -1;

        m_Data->InstanceVertexBuffer->SetData(&data, sizeof(InstanceData));

        outlineShader->Bind();
        mesh->GetVertexArray()->Bind();
        m_Data->InstanceVertexBuffer->Bind();

        const auto& submesh = mesh->GetSubmeshes()[submeshIndex];
        RenderCommand::DrawIndexedInstanced(mesh->GetVertexArray(), m_Data->InstanceVertexBuffer, 1, submesh.IndexCount, submesh.BaseIndex);
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
        OPTICK_EVENT();

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
        OPTICK_EVENT();

        FlushShadows();

        if (m_Data->CurrentRenderTarget) m_Data->CurrentRenderTarget->Bind();
        else RenderCommand::BindDefaultRenderTarget();

        RenderCommand::SetViewport(0, 0,
            m_Data->CurrentRenderTarget ? m_Data->CurrentRenderTarget->GetSpecification().Width : 1280,
            m_Data->CurrentRenderTarget ? m_Data->CurrentRenderTarget->GetSpecification().Height : 720);

        m_Data->ShadowData.ShadowTarget->BindRead(8);

        RenderCommand::SetDepthMask(true);
        RenderCommand::SetBlend(false);

        std::sort(m_Data->OpaqueQueue.begin(), m_Data->OpaqueQueue.end(),
            [](const RenderCommandPacket& a, const RenderCommandPacket& b)
            {
                if (a.SortKey != b.SortKey)
                    return a.SortKey < b.SortKey;

                return a.DistanceToCamera < b.DistanceToCamera;
            });

        ExecuteQueue(m_Data->OpaqueQueue);

        RenderCommand::SetDepthMask(false);
        RenderCommand::SetBlend(true);

        RenderCommand::SetBlendFunc(RendererAPI::BlendFactor::SrcAlpha, RendererAPI::BlendFactor::OneMinusSrcAlpha);

        std::sort(m_Data->TransparentQueue.begin(), m_Data->TransparentQueue.end(),
            [](const RenderCommandPacket& a, const RenderCommandPacket& b)
            {
                if (glm::abs(a.DistanceToCamera - b.DistanceToCamera) < 0.001f)
                    return a.EntityID < b.EntityID;

                return a.DistanceToCamera > b.DistanceToCamera;
            });

        ExecuteQueue(m_Data->TransparentQueue);

        RenderCommand::SetDepthMask(true);
        RenderCommand::SetBlend(false);

        if (!m_Data->LineVertices.empty())
        {
            m_Data->LineVBO->SetData(m_Data->LineVertices.data(), m_Data->LineVertices.size() * sizeof(LineVertex));

            m_Data->LineShader->Bind();
            m_Data->LineShader->SetMat4("u_ViewProjection", m_Data->ViewProjectionMatrix);

            RenderCommand::SetLineWidth(2.0f);
            RenderCommand::DrawLines(m_Data->LineVAO, m_Data->LineVertices.size());
        }
    }

    void Renderer::ExecuteQueue(const std::vector<RenderCommandPacket>& queue)
    {
        OPTICK_EVENT();

        if (queue.empty()) return;

        auto batchStart = queue.begin();

        static InstanceData currentBatchData[MaxInstances];
        uint32_t transformCount = 0;

        currentBatchData[transformCount].Transform = batchStart->Transform;
        currentBatchData[transformCount].EntityID = batchStart->EntityID;
        transformCount++;

        for (auto it = queue.begin() + 1; it != queue.end(); ++it)
        {
            bool isSameMesh = (it->Mesh == batchStart->Mesh && it->SubmeshIndex == batchStart->SubmeshIndex);
            bool isSameMaterial = (it->Material == batchStart->Material);

            if (isSameMesh && isSameMaterial && transformCount < MaxInstances)
            {
                currentBatchData[transformCount].Transform = it->Transform;
                currentBatchData[transformCount].EntityID = it->EntityID;
                transformCount++;
            }
            else
            {
                FlushBatch(batchStart->Mesh, batchStart->SubmeshIndex, batchStart->Material, currentBatchData, transformCount);

                batchStart = it;
                transformCount = 0;
                currentBatchData[transformCount].Transform = it->Transform;
                currentBatchData[transformCount].EntityID = it->EntityID;
                transformCount++;
            }
        }

        if (transformCount > 0)
            FlushBatch(batchStart->Mesh, batchStart->SubmeshIndex, batchStart->Material, currentBatchData, transformCount);
    }

    void Renderer::FlushBatch(const Ref<StaticMesh>& mesh, uint32_t submeshIndex, const Ref<Material>& material, const InstanceData* instanceData, uint32_t count)
    {
        OPTICK_EVENT();
        if (count == 0) return;

        m_Data->InstanceVertexBuffer->SetData(instanceData, count * sizeof(InstanceData));
        material->Bind();

        Ref<Shader> shader = material->GetShader();
        if (m_Data->CurrentShaderID != shader->GetRendererID())
        {
            shader->Bind();
            m_Data->CurrentShaderID = shader->GetRendererID();
            shader->SetMat4("u_ViewProjection", m_Data->ViewProjectionMatrix);
            shader->SetFloat3("u_CameraPosition", m_Data->CameraPosition);
            shader->SetMat4("u_View", m_Data->ViewMatrix);
        }

        mesh->GetVertexArray()->Bind();
        m_Data->InstanceVertexBuffer->Bind();

        const auto& submesh = mesh->GetSubmeshes()[submeshIndex];

        RenderCommand::DrawIndexedInstanced(mesh->GetVertexArray(), m_Data->InstanceVertexBuffer, count, submesh.IndexCount, submesh.BaseIndex);

        m_Data->Stats.DrawCalls++;
        m_Data->Stats.Instances += count;
        m_Data->Stats.TotalIndices += submesh.IndexCount * count;
    }

    void Renderer::FlushShadows()
    {
        OPTICK_EVENT();

        m_Data->ShadowData.ShadowTarget->BindWrite();
        RenderCommand::Clear();
        RenderCommand::SetCullFace(RendererAPI::CullFace::Front);

        m_Data->ShadowData.ShadowShader->Bind();

        auto& matrices = m_Data->ShadowData.BufferLocal.LightSpaceMatrices;
        for (int i = 0; i < 4; i++)
        {
            m_Data->ShadowData.ShadowShader->SetMat4("u_LightSpaceMatrices[" + std::to_string(i) + "]", matrices[i]);
        }

        if (!m_Data->ShadowQueue.empty())
        {
            auto batchStart = m_Data->ShadowQueue.begin();
            static InstanceData batchData[MaxInstances];
            uint32_t transformCount = 0;

            batchData[transformCount].Transform = batchStart->Transform;
            batchData[transformCount].EntityID = batchStart->EntityID;
            transformCount++;

            for (auto it = m_Data->ShadowQueue.begin() + 1; it != m_Data->ShadowQueue.end(); ++it)
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
                    m_Data->InstanceVertexBuffer->SetData(batchData, transformCount * sizeof(InstanceData));
                    batchStart->Mesh->GetVertexArray()->Bind();

                    const auto& submesh = batchStart->Mesh->GetSubmeshes()[batchStart->SubmeshIndex];

                    RenderCommand::DrawIndexedInstanced(batchStart->Mesh->GetVertexArray(), m_Data->InstanceVertexBuffer, transformCount, submesh.IndexCount, submesh.BaseIndex);

                    m_Data->Stats.DrawCalls++;
                    m_Data->Stats.Instances += transformCount;
                    m_Data->Stats.TotalIndices += submesh.IndexCount * transformCount;

                    batchStart = it;
                    transformCount = 0;
                    batchData[transformCount].Transform = it->Transform;
                    batchData[transformCount].EntityID = it->EntityID;
                    transformCount++;
                }
            }

            if (transformCount > 0)
            {
                m_Data->InstanceVertexBuffer->SetData(batchData, transformCount * sizeof(InstanceData));
                batchStart->Mesh->GetVertexArray()->Bind();

                const auto& submesh = batchStart->Mesh->GetSubmeshes()[batchStart->SubmeshIndex];

                RenderCommand::DrawIndexedInstanced(batchStart->Mesh->GetVertexArray(), m_Data->InstanceVertexBuffer, transformCount, submesh.IndexCount, submesh.BaseIndex);

                m_Data->Stats.DrawCalls++;
                m_Data->Stats.Instances += transformCount;
                m_Data->Stats.TotalIndices += submesh.IndexCount * transformCount;
            }
        }

        RenderCommand::SetCullFace(RendererAPI::CullFace::Back);
    }

    void Renderer::ExecutePickingPass(const Ref<Shader>& pickingShader)
    {
        OPTICK_EVENT();

        auto drawQueue = [&](const std::vector<RenderCommandPacket>& queue)
            {
                if (queue.empty()) return;

                auto batchStart = queue.begin();
                static InstanceData batchData[MaxInstances];
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
                        m_Data->InstanceVertexBuffer->SetData(batchData, transformCount * sizeof(InstanceData));

                        batchStart->Mesh->GetVertexArray()->Bind();
                        m_Data->InstanceVertexBuffer->Bind();

                        const auto& submesh = batchStart->Mesh->GetSubmeshes()[batchStart->SubmeshIndex];
                        RenderCommand::DrawIndexedInstanced(batchStart->Mesh->GetVertexArray(), m_Data->InstanceVertexBuffer, transformCount, submesh.IndexCount, submesh.BaseIndex);

                        batchStart = it;
                        transformCount = 0;
                        batchData[transformCount].Transform = it->Transform;
                        batchData[transformCount].EntityID = it->EntityID;
                        transformCount++;
                    }
                }

                if (transformCount > 0)
                {
                    m_Data->InstanceVertexBuffer->SetData(batchData, transformCount * sizeof(InstanceData));

                    batchStart->Mesh->GetVertexArray()->Bind();
                    m_Data->InstanceVertexBuffer->Bind();

                    const auto& submesh = batchStart->Mesh->GetSubmeshes()[batchStart->SubmeshIndex];
                    RenderCommand::DrawIndexedInstanced(batchStart->Mesh->GetVertexArray(), m_Data->InstanceVertexBuffer, transformCount, submesh.IndexCount, submesh.BaseIndex);
                }
            };

        drawQueue(m_Data->OpaqueQueue);
        drawQueue(m_Data->TransparentQueue);
    }

    bool Renderer::IsSphereVisibleToShadows(const glm::vec3& center, float radius)
    {
        for (int i = 0; i < 4; i++)
        {
            glm::vec4 centerNDC = m_Data->ShadowData.BufferLocal.LightSpaceMatrices[i] * glm::vec4(center, 1.0f);

            float rX = radius * glm::abs(m_Data->ShadowData.BufferLocal.LightSpaceMatrices[i][0][0]);
            float rY = radius * glm::abs(m_Data->ShadowData.BufferLocal.LightSpaceMatrices[i][1][1]);
            float rZ = radius * glm::abs(m_Data->ShadowData.BufferLocal.LightSpaceMatrices[i][2][2]);

            if (glm::abs(centerNDC.x) <= 1.0f + rX && glm::abs(centerNDC.y) <= 1.0f + rY &&
                centerNDC.z >= -1.0f - rZ && centerNDC.z <= 1.0f + rZ)
            {
                return true;
            }
        }
        return false;
    }

    void Renderer::SubmitShadowCaster(const Ref<StaticMesh>& mesh, uint32_t submeshIndex, const glm::mat4& transform, int entityID)
    {
        RenderCommandPacket packet;
        packet.Mesh = mesh;
        packet.SubmeshIndex = submeshIndex;
        packet.Transform = transform;
        packet.EntityID = entityID;

        m_Data->ShadowQueue.push_back(packet);
    }
}