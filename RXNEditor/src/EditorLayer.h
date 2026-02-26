#pragma once
#include <RXNEngine.h>

#include "SceneHierarchyPanel.h"
#include "ContentBrowserPanel.h"
#include "EnvironmentPanel.h"

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
		void OnScenePlay();
		void OnSceneSimulate(); 
		void OnSceneStop();

		Ray CastRayFromMouse(float mx, float my);
	private:
		enum class SceneState
		{
			Edit = 0, Play = 1, Simulate = 2
		};

	private:
		Ref<SceneRenderer> m_SceneRenderer;
		Ref<EditorCamera> m_EditorCamera;

		Ref<Scene> m_ActiveScene;
		Ref<Scene> m_EditorScene;
		std::string m_ActiveScenePath;

		uint32_t m_FPS = 0;

		uint32_t m_GizmoType = -1;

		SceneHierarchyPanel m_SceneHierarchyPanel;
		ContentBrowserPanel m_ContentBrowserPanel;
		EnvironmentPanel m_EnvironmentPanel;

		glm::vec2 m_ViewportBounds[2];
		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;

		SceneState m_SceneState = SceneState::Edit;

		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;
	};
}