#pragma once

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

    class Renderer
    {
    public:
        static void Init();
        static void Shutdown();

        static void OnWindowResize(uint32_t width, uint32_t height);

        static void BeginScene(const Camera& camera, const glm::mat4& transform, const LightEnvironment& lights,
            const Ref<Cubemap>& environment = nullptr, const Ref<RenderTarget>& renderTarget = nullptr);

        static void BeginScene(const EditorCamera& camera, const LightEnvironment& lights,
            const Ref<Cubemap>& environment = nullptr, const Ref<RenderTarget>& renderTarget = nullptr);

        static void EndScene();

        static void Submit(const Ref<StaticMesh>& mesh, uint32_t submeshIndex, const Ref<Material>& material, const glm::mat4& transform, int entityID = -1);

        static void DrawSkybox(const Ref<Cubemap>& skybox, const EditorCamera& camera);
        static void DrawSkybox(const Ref<Cubemap>& skybox, const Camera& camera, const glm::mat4& cameraTransform);
        static void DrawSkybox(const Ref<Cubemap>& skybox, const glm::mat4& cameraViewMatrix, const glm::mat4& cameraProjectionMatrix);

        static void DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color);
        static void DrawWireBox(const glm::mat4& transform, const glm::vec4& color);
        static void DrawWireSphere(const glm::mat4& transform, const glm::vec4& color);
        static void DrawWireCapsule(const glm::mat4& transform, float radius, float height, const glm::vec4& color);

        static void DrawEntityOutline(const Ref<StaticMesh>& mesh, uint32_t submeshIndex, const glm::mat4& transform, const Ref<Shader>& outlineShader);

        static void ExecutePickingPass(const Ref<Shader>& pickingShader);

        static bool IsSphereVisibleToShadows(const glm::vec3& center, float radius);
        static void SubmitShadowCaster(const Ref<StaticMesh>& mesh, uint32_t submeshIndex, const glm::mat4& transform, int entityID);

        static RendererStatistics GetStats();
        static void ResetStats();

        static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }
    private:
        static void PrepareScene(const glm::mat4& viewProjection, const glm::mat4& viewMatrix, const glm::vec3& cameraPosition, float cameraFOV,
            const LightEnvironment& lights, const Ref<Cubemap>& environment, const Ref<RenderTarget>& renderTarget);
        static void ExecuteQueue(const std::vector<RenderCommandPacket>& queue);
        static void FlushBatch(const Ref<StaticMesh>& mesh, uint32_t submeshIndex, const Ref<Material>& material, const InstanceData* instanceData, uint32_t count);
        static void Flush();
        static void FlushShadows();
    };
}