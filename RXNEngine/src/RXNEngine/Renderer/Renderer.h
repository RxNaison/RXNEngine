#pragma once

#include "Camera.h"
#include "Mesh.h"
#include "Material.h"
#include "Light.h"
#include "RenderTarget.h"
#include "EditorCamera.h"
#include "RendererAPI.h"
#include "Model.h"

#include <vector>

namespace RXNEngine {

    struct RenderCommandPacket
    {
        Ref<Mesh> Mesh;
        Ref<Material> Material;
        glm::mat4 Transform;
        float DistanceToCamera;
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

        static void Submit(const Ref<Mesh>& mesh, const Ref<Material>& material, const glm::mat4& transform = glm::mat4(1.0f));

        static void DrawSkybox(const Ref<Cubemap>& skybox, const EditorCamera& camera);
        static void DrawSkybox(const Ref<Cubemap>& skybox, const Camera& camera, const glm::mat4& cameraTransform);
        static void DrawSkybox(const Ref<Cubemap>& skybox, const glm::mat4& cameraViewMatrix, const glm::mat4& cameraProjectionMatrix);

        static void SubmitMesh(const Model& model, const glm::mat4& transform, const Ref<Material>& material = nullptr);

        static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }
    private:
        static void PrepareScene(const glm::mat4& viewProjection, const glm::mat4& viewMatrix, const glm::vec3& cameraPosition, float cameraFOV,
            const LightEnvironment& lights, const Ref<Cubemap>& environment, const Ref<RenderTarget>& renderTarget);
        static void ExecuteQueue(const std::vector<RenderCommandPacket>& queue);
        static void FlushBatch(const Ref<Mesh>& mesh, const Ref<Material>& material, const std::vector<glm::mat4>& transforms);
        static void Flush();
        static void FlushShadows();
    };
}