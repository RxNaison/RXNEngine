#pragma once

#include "RXNEngine/Scene/Scene.h"
#include "RenderTarget.h"
#include "EditorCamera.h"
#include "VertexArray.h"
#include "Shader.h"

namespace RXNEngine {

    struct SceneRendererSpecification
    {
        bool SwapChainTarget = false;
    };

    struct BloomMip
    {
        glm::vec2 Size;
        Ref<RenderTarget> Target;
    };

    class SceneRenderer
    {
    public:
        struct Settings
        {
            float Exposure = 1.0f;
            float Gamma = 2.2f;
            bool AutoExposure = false; //TODO
            bool ShowBoundingBoxes = false; //TODO
            bool ShowColliders = false;

            float BloomThreshold = 1.0f;
            float BloomKnee = 0.1f;
            float BloomIntensity = 0.04f;
            float BloomFilterRadius = 0.005f;
        };
    public:
        SceneRenderer(Ref<Scene> scene, const SceneRendererSpecification& spec = SceneRendererSpecification());
        ~SceneRenderer();

        void Init();
        void SetViewportSize(uint32_t width, uint32_t height);

        void SetScene(Ref<Scene>& scene) { m_Scene = scene; }

        void RenderEditor(EditorCamera& camera);
        void RenderRuntime();

        Ref<RenderTarget> GetFinalPass() { return m_FinalPass; }
        uint32_t GetFinalColorAttachmentRendererID() { return m_FinalPass->GetColorAttachmentRendererID(); }

        Settings& GetSettings() { return m_Settings; }
        int GetEntityIDAtMouse(int x, int y, const EditorCamera& camera);

     private:
        void RenderPostProcess();
        void RenderBloom();
    private:
        Ref<Scene> m_Scene;
        SceneRendererSpecification m_Specification;
        Settings m_Settings;

        Ref<RenderTarget> m_GeoPass;
        Ref<RenderTarget> m_FinalPass;

        Ref<RenderTarget> m_PickingPass;
        Ref<Shader> m_PickingShader;

        Ref<Shader> m_PostProcessShader;
        Ref<VertexArray> m_ScreenQuadVAO;

        std::vector<BloomMip> m_BloomMips;
        Ref<Shader> m_BloomDownsampleShader;
        Ref<Shader> m_BloomUpsampleShader;

        uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
    };
}