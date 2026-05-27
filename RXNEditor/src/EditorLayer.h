#pragma once
#include <RXNEngine.h>

#include "SceneHierarchyPanel.h"
#include "ContentBrowserPanel.h"
#include "EnvironmentPanel.h"
#include "MaterialEditorPanel.h"
#include "PhysicsMaterialEditorPanel.h"
#include "LauncherPanel.h"
#include "RXNEngine/Asset/ModelImporter.h"
#include "CommandHistory.h"

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
		void DropSelectedToFloor();
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
		MaterialEditorPanel m_MaterialEditorPanel;
		PhysicsMaterialEditorPanel m_PhysicsMaterialEditorPanel;
		LauncherPanel m_LauncherPanel;

		glm::vec2 m_ViewportBounds[2];
		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;
		bool m_AllowViewportCamera = false;

		SceneState m_SceneState = SceneState::Edit;

		uint32_t m_ViewportWidth = 0;
		uint32_t m_ViewportHeight = 0;

		std::string m_PendingImportPath;
		bool m_ShowImportDialog = false;
		ModelImportSettings m_ImportSettings;

		Ref<RenderTarget> m_PIPRenderTarget;
		Ref<SceneRenderer> m_PIPRenderer;
		Ref<Texture2D> m_IconCamera;
		Ref<Texture2D> m_IconDirLight;
		Ref<Texture2D> m_IconPointLight;
		Ref<Texture2D> m_IconSpotLight;
		Ref<Texture2D> m_IconAudio;

		struct IconHitbox
		{
			Entity Ent;
			ImVec2 Min;
			ImVec2 Max;
		};
		std::vector<IconHitbox> m_IconHitboxes;

		bool m_WasGizmoUsing = false;
		std::vector<std::pair<RXNEngine::UUID, RXNEngine::TransformComponent>> m_GizmoStartTransforms;
		std::vector<std::pair<RXNEngine::UUID, RXNEngine::UITransformComponent>> m_GizmoStartUITransforms;

	private:
		void OpenBuildDialog();
		bool PerformBuild(const std::string& buildPath, const std::string& startScenePath);

	private:
		bool m_ShowBuildModal = false;
		bool m_IsBuilding = false;
		bool m_BuildSuccess = false;
		std::string m_BuildPath = "";
		std::string m_BuildExeName = "";
		std::string m_BuildStartScene = "";
		std::string m_BuildError = "";
	};
}