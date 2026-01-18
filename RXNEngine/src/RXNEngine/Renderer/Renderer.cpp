#include "rxnpch.h"
#include "Renderer.h"
#include "RXNEngine/Core/Math.h"
#include "Frustum.h"
#include "UniformBuffer.h"
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

    struct RendererData
    {
        Ref<Framebuffer> CurrentFramebuffer = nullptr;
        std::vector<RenderCommandPacket> OpaqueQueue;
        std::vector<RenderCommandPacket> TransparentQueue;
        glm::mat4 ViewProjectionMatrix;
        glm::mat4 ViewMatrix;
        glm::vec3 CameraPosition;
        Frustum CameraFrustum;
        float CameraFOV = 45.0f;

        uint32_t CurrentShaderID = 0;
        uint32_t CurrentVertexArrayID = 0;

        std::array<uint32_t, 32> TextureSlots{ 0 };

        Ref<VertexBuffer> InstanceVertexBuffer;
        glm::mat4* InstanceBufferBase = nullptr;
        uint32_t InstanceCount = 0;
        const uint32_t MaxInstances = 10000;

        Ref<UniformBuffer> LightUniformBuffer;
        LightDataGPU LightBufferLocal;

        Ref<VertexArray> SkyboxVAO;
        Ref<Shader> SkyboxShader;

        Ref<TextureCube> SceneEnvironment;
		ShadowData ShadowData;
    };

    static RendererData s_Data;

    std::vector<glm::vec4> GetFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view)
    {
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

    void CalculateShadowMapMatrices(const glm::mat4& cameraView, const glm::vec3& lightDir)
    {
        float aspect = (float)s_Data.CurrentFramebuffer->GetSpecification().Width / (float)s_Data.CurrentFramebuffer->GetSpecification().Height;
        float fov = s_Data.CameraFOV < 0.01f ? glm::radians(45.0f) : s_Data.CameraFOV;
        float nearPlane = 0.1f;

        for (size_t i = 0; i < 4; ++i)
        {
            float pNear = (i == 0) ? nearPlane : s_Data.ShadowData.CascadeSplits[i - 1];
            float pFar = s_Data.ShadowData.CascadeSplits[i];

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

            // Tune Z bounds to include casters behind the camera slice
            float zMult = 10.0f;
            if (minZ < 0) minZ *= zMult; else minZ /= zMult;
            if (maxZ < 0) maxZ /= zMult; else maxZ *= zMult;

            const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);

            s_Data.ShadowData.BufferLocal.LightSpaceMatrices[i] = lightProjection * lightView;
            s_Data.ShadowData.BufferLocal.CascadePlaneDistances[i].x = pFar;
        }

        s_Data.ShadowData.ShadowUniformBuffer->SetData(&s_Data.ShadowData.BufferLocal, sizeof(ShadowData::ShadowDataGPU));
    }

    void Renderer::Init()
    {
        RenderCommand::Init();
        s_Data.OpaqueQueue.reserve(1000);
        s_Data.InstanceVertexBuffer = VertexBuffer::Create(s_Data.MaxInstances * sizeof(glm::mat4));
        s_Data.InstanceVertexBuffer->SetLayout({
            { ShaderDataType::Float4, "a_ModelMatrixCol0", false, true },
            { ShaderDataType::Float4, "a_ModelMatrixCol1", false, true },
            { ShaderDataType::Float4, "a_ModelMatrixCol2", false, true },
            { ShaderDataType::Float4, "a_ModelMatrixCol3", false, true }
            });
        s_Data.LightUniformBuffer = UniformBuffer::Create(sizeof(LightDataGPU), 1);

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
        s_Data.SkyboxVAO = VertexArray::Create();
        s_Data.SkyboxVAO->AddVertexBuffer(skyboxVB);

        s_Data.SkyboxShader = Shader::Create("assets/shaders/skybox.glsl");

        s_Data.ShadowData.ShadowTarget = ShadowMap::Create(4096);
        s_Data.ShadowData.ShadowTarget->Init(4096);

        s_Data.ShadowData.ShadowShader = Shader::Create("assets/shaders/shadow_depth.glsl");

        s_Data.ShadowData.ShadowUniformBuffer = UniformBuffer::Create(sizeof(ShadowData::ShadowDataGPU), 2);

        s_Data.ShadowData.CascadeSplits = { 7.0f, 25.0f, 90.0f, 1000.0f };
    }

    void Renderer::Shutdown()
    {
        s_Data.OpaqueQueue.clear();
    }

    void Renderer::OnWindowResize(uint32_t width, uint32_t height)
    {
        RenderCommand::SetViewport(0, 0, width, height);
    }

    void Renderer::BeginScene(const EditorCamera& camera, const LightEnvironment& lights,
        const Ref<TextureCube>& environment, const Ref<Framebuffer>& targetFramebuffer)
    {
        PrepareScene(camera.GetViewProjection(), camera.GetViewMatrix(), camera.GetPosition(), camera.GetFOV(), lights, environment, targetFramebuffer);
    }

    void Renderer::BeginScene(const Camera& camera, const glm::mat4& transform, const LightEnvironment& lights,
        const Ref<TextureCube>& environment, const Ref<Framebuffer>& targetFramebuffer)
    {
        glm::mat4 viewProj = camera.GetProjection() * glm::inverse(transform);
        glm::mat4 view = glm::inverse(transform);
        glm::vec3 cameraPos = glm::vec3(transform[3]);
        float fov = 2.0f * glm::atan(1.0f / camera.GetProjection()[1][1]);

        PrepareScene(viewProj, view, cameraPos, fov, lights, environment, targetFramebuffer);
    }

    void Renderer::PrepareScene(
        const glm::mat4& viewProjection,
        const glm::mat4& viewMatrix,
        const glm::vec3& cameraPosition,
        float cameraFOV,
        const LightEnvironment& lights,
        const Ref<TextureCube>& environment,
        const Ref<Framebuffer>& targetFramebuffer)
    {

        if (environment)
        {
            s_Data.SceneEnvironment = environment;

            RenderCommand::BindTextureID(10, environment->GetIrradianceRendererID());
            RenderCommand::BindTextureID(11, environment->GetPrefilterRendererID());
            RenderCommand::BindTextureID(12, environment->GetBRDFLUTRendererID());
        }

        s_Data.CurrentFramebuffer = targetFramebuffer;

        if (s_Data.CurrentFramebuffer)
        {
            s_Data.CurrentFramebuffer->Bind();
            RenderCommand::SetViewport(0, 0,
                s_Data.CurrentFramebuffer->GetSpecification().Width,
                s_Data.CurrentFramebuffer->GetSpecification().Height);
            RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
            RenderCommand::Clear();
        }
        else
        {
			RenderCommand::BindDefaultFramebuffer();

            //RenderCommand::SetViewport(0, 0, s_Data.WindowWidth, s_Data.WindowHeight);

            RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 1.0f });
            RenderCommand::Clear();
        }

        s_Data.ViewProjectionMatrix = viewProjection;
        s_Data.ViewMatrix = viewMatrix;
        s_Data.CameraPosition = cameraPosition;
        s_Data.CameraFrustum.Define(viewProjection);

        s_Data.LightBufferLocal.DirLightDirection = glm::vec4(lights.DirLight.Direction, lights.DirLight.Intensity);
        s_Data.LightBufferLocal.DirLightColor = glm::vec4(lights.DirLight.Color, 0.0f);
        s_Data.LightBufferLocal.PointLightCount = std::min((uint32_t)lights.PointLights.size(), 100u);

        for (uint32_t i = 0; i < s_Data.LightBufferLocal.PointLightCount; i++)
        {
            const auto& light = lights.PointLights[i];
            s_Data.LightBufferLocal.PointLights[i].Position = glm::vec4(light.Position, light.Intensity);
            s_Data.LightBufferLocal.PointLights[i].Color = glm::vec4(light.Color, light.Radius);
            s_Data.LightBufferLocal.PointLights[i].Falloff = glm::vec4(light.Falloff, 0.0f, 0.0f, 0.0f);
        }

        s_Data.LightUniformBuffer->SetData(&s_Data.LightBufferLocal, sizeof(LightDataGPU));

        s_Data.OpaqueQueue.clear();
        s_Data.TransparentQueue.clear();

        // reset state at start of frame? 
        s_Data.CurrentShaderID = 0;
        s_Data.CurrentVertexArrayID = 0;
        std::fill(s_Data.TextureSlots.begin(), s_Data.TextureSlots.end(), 0);
    }

    void Renderer::Submit(const Ref<Mesh>& mesh, const Ref<Material>& material, const glm::mat4& transform)
    {
        AABB worldAABB = Math::CalculateWorldAABB(mesh->GetAABB(), transform);

        if (!s_Data.CameraFrustum.IsBoxVisible(worldAABB.Min, worldAABB.Max))
            return;

        RenderCommandPacket packet;
        packet.Mesh = mesh;
        packet.Material = material;
        packet.Transform = transform;

        float dist = glm::distance(s_Data.CameraPosition, (worldAABB.Min + worldAABB.Max) * 0.5f);
        packet.DistanceToCamera = dist * dist;

        if (material->IsTransparent())
            s_Data.TransparentQueue.push_back(packet);
        else
            s_Data.OpaqueQueue.push_back(packet);
    }

    void Renderer::EndScene()
    {
        Flush();

        if (s_Data.CurrentFramebuffer)
        {
            s_Data.CurrentFramebuffer->Unbind();
            s_Data.CurrentFramebuffer = nullptr;
        }
    }

    void Renderer::DrawModel(const Model& model, const glm::mat4& transform)
    {
        for (const auto& submesh : model.GetSubmeshes())
        {
            glm::mat4 finalTransform = transform * submesh.LocalTransform;

            Renderer::Submit(submesh.Geometry, submesh.Surface, finalTransform);
        }
    }

    void Renderer::DrawSkybox(const Ref<TextureCube>& skybox, const EditorCamera& camera)
    {
        DrawSkybox(skybox, camera.GetViewMatrix(), camera.GetProjection());
    }

    void Renderer::DrawSkybox(const Ref<TextureCube>& skybox, const Camera& camera, const glm::mat4& transform)
    {
        DrawSkybox(skybox, glm::inverse(transform), camera.GetProjection());
    }

    void Renderer::DrawSkybox(const Ref<TextureCube>& skybox, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
    {
        RenderCommand::SetDepthFunc(RendererAPI::DepthFunc::LessEqual);

        s_Data.SkyboxShader->Bind();

        glm::mat4 view = glm::mat4(glm::mat3(viewMatrix));
        glm::mat4 projection = projectionMatrix;

        s_Data.SkyboxShader->SetMat4("u_ViewProjection", projection * view);

        skybox->Bind(0);
        s_Data.SkyboxShader->SetInt("u_Skybox", 0);

        s_Data.SkyboxVAO->Bind();
        RenderCommand::Draw(s_Data.SkyboxVAO, 36);
        s_Data.SkyboxVAO->Unbind();

        RenderCommand::SetDepthFunc(RendererAPI::DepthFunc::Less);
    }

    void Renderer::Flush()
    {
        CalculateShadowMapMatrices(s_Data.ViewMatrix, s_Data.LightBufferLocal.DirLightDirection);

        FlushShadows();

        if (s_Data.CurrentFramebuffer) s_Data.CurrentFramebuffer->Bind();
        else RenderCommand::BindDefaultFramebuffer();

        RenderCommand::SetViewport(0, 0, 
            s_Data.CurrentFramebuffer ? s_Data.CurrentFramebuffer->GetSpecification().Width : 1280, 
            s_Data.CurrentFramebuffer ? s_Data.CurrentFramebuffer->GetSpecification().Height : 720);

        s_Data.ShadowData.ShadowTarget->BindRead(8);

        std::sort(s_Data.OpaqueQueue.begin(), s_Data.OpaqueQueue.end(),
            [](const RenderCommandPacket& a, const RenderCommandPacket& b)
            {
                if (a.Material->GetShader()->GetRendererID() != b.Material->GetShader()->GetRendererID())
                    return a.Material->GetShader()->GetRendererID() < b.Material->GetShader()->GetRendererID();

                return a.DistanceToCamera < b.DistanceToCamera;
            });

        ExecuteQueue(s_Data.OpaqueQueue);

        std::sort(s_Data.TransparentQueue.begin(), s_Data.TransparentQueue.end(),
            [](const RenderCommandPacket& a, const RenderCommandPacket& b)
            {
                return a.DistanceToCamera > b.DistanceToCamera;
            });

        ExecuteQueue(s_Data.TransparentQueue);
    }

    void Renderer::ExecuteQueue(const std::vector<RenderCommandPacket>& queue)
    {
        if (queue.empty()) return;

        auto batchStart = queue.begin();
        std::vector<glm::mat4> currentBatchTransforms;
        currentBatchTransforms.reserve(s_Data.MaxInstances);
        currentBatchTransforms.push_back(batchStart->Transform);

        for (auto it = queue.begin() + 1; it != queue.end(); ++it)
        {
            bool isSameMesh = (it->Mesh->GetVertexArray() == batchStart->Mesh->GetVertexArray());
            bool isSameMaterial = (it->Material->GetShader() == batchStart->Material->GetShader());
            // Note: check Material Instance ID too, not just Shader!

            if (isSameMesh && isSameMaterial && currentBatchTransforms.size() < s_Data.MaxInstances)
            {
                currentBatchTransforms.push_back(it->Transform);
            }
            else
            {
                FlushBatch(batchStart->Mesh, batchStart->Material, currentBatchTransforms);

                batchStart = it;
                currentBatchTransforms.clear();
                currentBatchTransforms.push_back(it->Transform);
            }
        }

        if (!currentBatchTransforms.empty())
            FlushBatch(batchStart->Mesh, batchStart->Material, currentBatchTransforms);
    }

    void Renderer::FlushBatch(const Ref<Mesh>& mesh, const Ref<Material>& material, const std::vector<glm::mat4>& transforms)
    {
        if (transforms.empty()) return;

        s_Data.InstanceVertexBuffer->SetData(transforms.data(), transforms.size() * sizeof(glm::mat4));

        Ref<Shader> shader = material->GetShader();
        if (s_Data.CurrentShaderID != shader->GetRendererID())
        {
            shader->Bind();
            s_Data.CurrentShaderID = shader->GetRendererID();
            shader->SetMat4("u_ViewProjection", s_Data.ViewProjectionMatrix);
            shader->SetFloat3("u_CameraPosition", s_Data.CameraPosition);
            shader->SetMat4("u_View", s_Data.ViewMatrix);
        }

        for (auto& [name, val] : material->m_UniformsInt)
            shader->SetInt(name, val);
        for (auto& [name, val] : material->m_UniformsFloat)
            shader->SetFloat(name, val);
        for (auto& [name, val] : material->m_UniformsFloat3)
            shader->SetFloat3(name, val);
        for (auto& [name, val] : material->m_UniformsFloat4)
            shader->SetFloat4(name, val);
        for (auto& [name, val] : material->m_UniformsMat4)
            shader->SetMat4(name, val);

        uint32_t slotCounter = 0;
        for (auto& [name, texture] : material->m_Textures)
        {
            if (slotCounter >= 31) break;
            uint32_t texID = texture->GetRendererID();
            if (s_Data.TextureSlots[slotCounter] != texID)
            {
                texture->Bind(slotCounter);
                s_Data.TextureSlots[slotCounter] = texID;
            }
            shader->SetInt(name, slotCounter);
            slotCounter++;
        }

        mesh->GetVertexArray()->Bind();

        s_Data.InstanceVertexBuffer->Bind();

        RenderCommand::DrawIndexedInstanced(mesh->GetVertexArray(), s_Data.InstanceVertexBuffer, transforms.size());
    }

    void Renderer::FlushShadows()
    {
        s_Data.ShadowData.ShadowTarget->BindWrite();
        RenderCommand::Clear();
        RenderCommand::SetCullFace(RendererAPI::CullFace::Front);

        s_Data.ShadowData.ShadowShader->Bind();

        auto& matrices = s_Data.ShadowData.BufferLocal.LightSpaceMatrices;
        for (int i = 0; i < 4; i++)
        {
            s_Data.ShadowData.ShadowShader->SetMat4("u_LightSpaceMatrices[" + std::to_string(i) + "]", matrices[i]);
        }

        if (!s_Data.OpaqueQueue.empty())
        {
            auto batchStart = s_Data.OpaqueQueue.begin();
            std::vector<glm::mat4> batchTransforms;
            batchTransforms.push_back(batchStart->Transform);

            for (auto it = s_Data.OpaqueQueue.begin() + 1; it != s_Data.OpaqueQueue.end(); ++it)
            {
                bool isSameMesh = (it->Mesh->GetVertexArray() == batchStart->Mesh->GetVertexArray());

                if (isSameMesh && batchTransforms.size() < s_Data.MaxInstances)
                {
                    batchTransforms.push_back(it->Transform);
                }
                else
                {
                    s_Data.InstanceVertexBuffer->SetData(batchTransforms.data(), batchTransforms.size() * sizeof(glm::mat4));
                    batchStart->Mesh->GetVertexArray()->Bind();
                    RenderCommand::DrawIndexedInstanced(batchStart->Mesh->GetVertexArray(), s_Data.InstanceVertexBuffer, batchTransforms.size());

                    batchStart = it;
                    batchTransforms.clear();
                    batchTransforms.push_back(it->Transform);
                }
            }

            if (!batchTransforms.empty())
            {
                s_Data.InstanceVertexBuffer->SetData(batchTransforms.data(), batchTransforms.size() * sizeof(glm::mat4));
                batchStart->Mesh->GetVertexArray()->Bind();
                RenderCommand::DrawIndexedInstanced(batchStart->Mesh->GetVertexArray(), s_Data.InstanceVertexBuffer, batchTransforms.size());
            }
        }

        RenderCommand::SetCullFace(RendererAPI::CullFace::Back);
    }
}