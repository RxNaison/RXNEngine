#pragma once

#include "RenderTarget.h"
#include "RXNEngine/Scene/Scene.h"
#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Scene/EditorCamera.h"
#include "RXNEngine/Renderer/GraphicsAPI/VertexArray.h"
#include "RXNEngine/Renderer/GraphicsAPI/Shader.h"
#include "RXNEngine/Renderer/ReflectionProbeCache.h"
#include "RXNEngine/Renderer/HiZBuffer.h"
#include "RXNEngine/Renderer/SoftwareOcclusion.h"

namespace RXNEngine {

    class Frustum;

    struct SceneRendererSpecification
    {
        bool SwapChainTarget = false;
        bool DisableMSAA = false;
        bool DisableAO = false;
        bool DisableBloom = false;
        bool DisablePicking = false;
        bool DisableOutline = false;
    };

    struct BloomMip
    {
        glm::vec2 Size;
        Ref<RenderTarget> Target;
    };

    class SceneRenderer
    {
    public:
        using Settings = SceneRendererSettings;
    public:
        SceneRenderer(Ref<Scene> scene, const SceneRendererSpecification& spec = SceneRendererSpecification());
        ~SceneRenderer();

        void Init();
        void SetViewportSize(uint32_t width, uint32_t height);

        void SetScene(Ref<Scene>& scene) { m_Scene = scene; }
        Ref<Scene>& GetScene() { return m_Scene; }

        void RenderEditor(uint32_t targetWidth, uint32_t targetHeight, float deltaTime, EditorCamera& camera, const std::vector<Entity>& selectedEntities = {});
        void RenderRuntime(uint32_t targetWidth, uint32_t targetHeight);
        void RenderToTarget(uint32_t width, uint32_t height, Camera& camera, const glm::mat4& cameraTransform);

        Ref<RenderTarget> GetFinalPass() { return m_FinalPass; }
        uint32_t GetFinalColorAttachmentRendererID() { return m_FinalPass->GetColorAttachmentRendererID(); }

        Settings& GetSettings() { return m_Scene ? m_Scene->m_RendererSettings : m_Settings; }
        const Settings& GetSettings() const { return m_Scene ? m_Scene->m_RendererSettings : m_Settings; }
        int GetEntityIDAtMouse(int x, int y, const EditorCamera& camera, const std::vector<Entity>& selectedEntities = {});

        void RenderPostProcess();
        void RenderBloom();
        void RenderGridOverlay(EditorCamera& camera);
        void ResolveMSAA();
        void RenderUI(const Camera& camera, const glm::mat4& cameraTransform, bool worldSpace, const std::vector<Entity>& selectedEntities = {});
        void RenderUIPicking(const EditorCamera& camera, const std::vector<Entity>& selectedEntities = {});
        void RenderScene(const Camera& camera, const glm::mat4& cameraTransform, bool showColliders, float deltaTime = 0.0f, const std::vector<Entity>& selectedEntities = {}, const Ref<RenderTarget>& captureTarget = nullptr);
        void CaptureReflectionProbes();

        uint32_t GetHiZOccludedCount() const { return m_LastHiZOccluded; }
    private:
        void RenderAO(const Camera& camera, const glm::mat4& cameraTransform);
        void RenderSSRDenoise();
    private:
        Ref<Scene> m_Scene;
        SceneRendererSpecification m_Specification;
        Settings m_Settings;

        Ref<RenderTarget> m_GeoPass;
        Ref<RenderTarget> m_ResolvePass;
        Ref<RenderTarget> m_PrevResolvePass;
        Ref<RenderTarget> m_FinalPass;
        Ref<RenderTarget> m_ProbeCapturePass;
        Ref<ReflectionProbeCache> m_ProbeCache;
        Ref<HiZBuffer> m_HiZ;
        bool m_ProbeStorageReady = false;

        Ref<RenderTarget> m_PickingPass;
        Ref<Shader> m_PickingShader;
        Ref<Shader> m_UIPickingShader;
        Ref<VertexArray> m_UIPickingQuadVAO;

        Ref<Shader> m_PostProcessShader;
        Ref<VertexArray> m_ScreenQuadVAO;

        std::vector<BloomMip> m_BloomMips;
        Ref<Shader> m_BloomDownsampleShader;
        Ref<Shader> m_BloomUpsampleShader;

        Ref<Shader> m_GridShader;
        Ref<VertexArray> m_GridQuadVAO;

        Ref<RenderTarget> m_OutlineMaskPass;
        Ref<Shader> m_OutlineMaskShader;

        Ref<RenderTarget> m_AOPass;
        Ref<RenderTarget> m_AOBlurPass;
        Ref<Shader> m_AOShader;
        Ref<Shader> m_AOBlurShader;
        Ref<RenderTarget> m_SSRBlurPass;
        Ref<RenderTarget> m_SSRPass;
        Ref<Shader> m_SSRDenoiseShader;

        uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
        bool m_IsFirstFrame = true;
        uint32_t m_LastHiZOccluded = 0;

        SoftwareOcclusionBuffer m_SoftOcclusion;
        bool RasterizeOccluders(const glm::mat4& viewProj, const Frustum& frustum, const glm::vec3& cameraPos);
        glm::mat4 m_CurrentViewProjection = glm::mat4(1.0f);
        glm::mat4 m_PrevViewProjection = glm::mat4(1.0f);
    };
}
