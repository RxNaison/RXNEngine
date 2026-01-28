#pragma once
#include <RXNEngine.h>

#include "SceneHierarchyPanel.h"

using namespace RXNEngine;

namespace RXNEditor {

	class EditorLayer : public Layer
	{
	public:
		EditorLayer(const std::string& name = "RXNEditor");
		virtual ~EditorLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(float deltaTime) override;
		virtual void OnFixedUpdate(float fixedDeltaTime) override;
		virtual void OnEvent(Event& event) override;
		virtual void OnImGuiRenderer() override;

	private:
		bool OnKeyPressed(KeyPressedEvent& e);

		void NewScene();
		void OpenScene(const std::string& path);
		void SaveSceneAs(const std::string& path);

		Ray CastRayFromMouse(float mx, float my);
		bool OnMouseButtonPressed(MouseButtonPressedEvent& e);

		void DrawAABB(const AABB& aabb, const glm::mat4& transform, const glm::vec4& color);

	private:
		Ref<RenderTarget> m_RenderTarget;
		Ref<Shader> m_ModelShader;
		Ref<EditorCamera> m_Camera;

		Ref<Scene> m_ActiveScene;
		Entity m_CameraEntity;
		Entity m_ModelEntity;
		Entity m_SkyboxEntity;

		uint32_t m_FPS = 0;

		uint32_t m_GizmoType = -1;

		SceneHierarchyPanel m_Panel;

		glm::vec2 m_ViewportBounds[2];
		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;
	};
}