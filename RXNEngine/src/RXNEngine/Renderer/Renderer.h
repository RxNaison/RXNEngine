#pragma once

#include "RXNEngine/Core/Subsystem.h"
#include "RendererAPI.h"
#include "RenderTarget.h"
#include "Light.h"
#include "RXNEngine/Scene/Camera.h"
#include "RXNEngine/Asset/StaticMesh.h"
#include "RXNEngine/Asset/Material.h"
#include "RXNEngine/Scene/EditorCamera.h"

#include <vector>

namespace RXNEngine {

    struct RenderCommandPacket
    {
        Ref<StaticMesh> Mesh = nullptr;
        Ref<Material> Material = nullptr;
        glm::mat4 Transform = {};
        uint32_t SubmeshIndex = 0;
        float DistanceToCamera = 0;

        uint64_t SortKey = 0;
        int EntityID = -1;
    };

    struct InstanceData
    {
        glm::mat4 Transform = {};
        int EntityID = -1;
    };

    struct RendererStatistics
    {
        uint32_t DrawCalls = 0;
        uint32_t Instances = 0;
        uint32_t TotalIndices = 0;

        void Reset() { DrawCalls = 0; Instances = 0; TotalIndices = 0; }
    };

    struct RendererData;

    class Renderer : public Subsystem
    {
    public:
        Renderer() = default;
        virtual ~Renderer() = default;

        virtual void Init() override;
        virtual void Update(float deltaTime) override {}
        virtual void Shutdown() override;

        void OnWindowResize(uint32_t width, uint32_t height);

        void BeginScene(const Camera& camera, const glm::mat4& transform, const LightEnvironment& lights,
            const Ref<Cubemap>& environment = nullptr, const Ref<RenderTarget>& renderTarget = nullptr);

        void BeginScene(const EditorCamera& camera, const LightEnvironment& lights,
            const Ref<Cubemap>& environment = nullptr, const Ref<RenderTarget>& renderTarget = nullptr);

        void EndScene();

        void Submit(const Ref<StaticMesh>& mesh, uint32_t submeshIndex, const Ref<Material>& material, const glm::mat4& transform, int entityID = -1);

        void DrawSkybox(const Ref<Cubemap>& skybox, const EditorCamera& camera);
        void DrawSkybox(const Ref<Cubemap>& skybox, const Camera& camera, const glm::mat4& cameraTransform);
        void DrawSkybox(const Ref<Cubemap>& skybox, const glm::mat4& cameraViewMatrix, const glm::mat4& cameraProjectionMatrix);

        void DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color);
        void DrawWireBox(const glm::mat4& transform, const glm::vec4& color);
        void DrawWireSphere(const glm::mat4& transform, const glm::vec4& color);
        void DrawWireCapsule(const glm::mat4& transform, float radius, float height, const glm::vec4& color);

        void DrawEntityOutline(const Ref<StaticMesh>& mesh, uint32_t submeshIndex, const glm::mat4& transform, const Ref<Shader>& outlineShader);

        void ExecutePickingPass(const Ref<Shader>& pickingShader);

        bool IsSphereVisibleToShadows(const glm::vec3& center, float radius);
        void SubmitShadowCaster(const Ref<StaticMesh>& mesh, uint32_t submeshIndex, const glm::mat4& transform, int entityID);

        RendererStatistics GetStats();
        void ResetStats();

        static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }

    private:
        void PrepareScene(const glm::mat4& viewProjection, const glm::mat4& viewMatrix, const glm::vec3& cameraPosition, float cameraFOV,
            const LightEnvironment& lights, const Ref<Cubemap>& environment, const Ref<RenderTarget>& renderTarget);

        void ExecuteQueue(const std::vector<const RenderCommandPacket*>& queue);
        void FlushBatch(const Ref<StaticMesh>& mesh, uint32_t submeshIndex, const Ref<Material>& material, const InstanceData* instanceData, uint32_t count);
        void Flush();
        void FlushShadows();

        void CalculateShadowMapMatrices(const glm::mat4& cameraView, const glm::vec3& lightDir);

    private:
        RendererData* m_Data = nullptr;
    };
}