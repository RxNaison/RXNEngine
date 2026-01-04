#include "rxnpch.h"
#include "Renderer.h"
#include "RXNEngine/Core/Math.h"
#include "Frustum.h"
#include "UniformBuffer.h"
#include "RenderCommand.h"

#include <algorithm>
#include <array>
// remove opengl dependency from here
#include <glad/glad.h>

namespace RXNEngine {

    struct LightDataGPU
    {
        glm::vec4 DirLightDirection;
        glm::vec4 DirLightColor;    

        uint32_t PointLightCount;
        uint32_t Padding[3];      // align to 16 bytes

        struct PointLightGPU {
            glm::vec4 Position;  // w = Intensity
            glm::vec4 Color;     // w = Radius
            glm::vec4 Falloff;   // x = Falloff, yzw = Padding
        } PointLights[100];
    };

    struct RendererData
    {
        Ref<Framebuffer> CurrentFramebuffer = nullptr;
        std::vector<RenderCommandPacket> OpaqueQueue;
        std::vector<RenderCommandPacket> TransparentQueue;
        glm::mat4 ViewProjectionMatrix;
        glm::vec3 CameraPosition;
        Frustum CameraFrustum;

        uint32_t CurrentShaderID = 0;
        uint32_t CurrentVertexArrayID = 0;

        std::array<uint32_t, 32> TextureSlots{ 0 };

        Ref<VertexBuffer> InstanceVertexBuffer;
        glm::mat4* InstanceBufferBase = nullptr;
        uint32_t InstanceCount = 0;
        const uint32_t MaxInstances = 10000;

        Ref<UniformBuffer> LightUniformBuffer;
        LightDataGPU LightBufferLocal;
    };

    static RendererData s_Data;

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
    }

    void Renderer::Shutdown()
    {
        s_Data.OpaqueQueue.clear();
    }

    void Renderer::OnWindowResize(uint32_t width, uint32_t height)
    {
        RenderCommand::SetViewport(0, 0, width, height);
    }

    void Renderer::BeginScene(const EditorCamera& camera,
        const LightEnvironment& lights, const Ref<Framebuffer>& targetFramebuffer)
    {
        PrepareScene(camera.GetViewProjection(), camera.GetPosition(), lights, targetFramebuffer);
    }

    void Renderer::BeginScene(const Camera& camera, const glm::mat4& transform, const LightEnvironment& lights,
        const Ref<Framebuffer>& targetFramebuffer)
    {
        glm::mat4 viewProj = camera.GetProjection() * glm::inverse(transform);
        glm::vec3 cameraPos = glm::vec3(transform[3]);

        PrepareScene(viewProj, cameraPos, lights, targetFramebuffer);
    }

    void Renderer::PrepareScene(const glm::mat4& viewProjection,
        const glm::vec3& cameraPosition,
        const LightEnvironment& lights,
        const Ref<Framebuffer>& targetFramebuffer)
    {
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
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            //RenderCommand::SetViewport(0, 0, s_Data.WindowWidth, s_Data.WindowHeight);

            RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 1.0f });
            RenderCommand::Clear();
        }

        s_Data.ViewProjectionMatrix = viewProjection;
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
        {
            return;
        }

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

    void Renderer::Flush()
    {
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
        for (int i = 0; i < 4; i++)
        {
            uint32_t loc = 4 + i; // Locations 4,5,6,7
            glEnableVertexAttribArray(loc);
            glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (const void*)(i * sizeof(glm::vec4)));
            glVertexAttribDivisor(loc, 1);
        }

        glDrawElementsInstanced(GL_TRIANGLES, mesh->GetIndexCount(), GL_UNSIGNED_INT, nullptr, transforms.size());
    }
}