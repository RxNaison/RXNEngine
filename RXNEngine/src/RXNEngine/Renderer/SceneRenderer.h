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

    class SceneRenderer
    {
    public:
        struct Settings
        {
            float Exposure = 1.0f;
            float Gamma = 2.2f;
            bool AutoExposure = false; //TODO
            bool ShowBoundingBoxes = false;
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

     private:
        void RenderGeometry(const Camera& camera, const glm::mat4& viewProj);
        void RenderPostProcess();

    private:
        Ref<Scene> m_Scene;
        SceneRendererSpecification m_Specification;
        Settings m_Settings;

        Ref<RenderTarget> m_GeoPass;
        Ref<RenderTarget> m_FinalPass;

        Ref<Shader> m_PostProcessShader;
        Ref<VertexArray> m_ScreenQuadVAO;

        uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
    };
}