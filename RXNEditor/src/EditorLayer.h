#pragma once
#include "RXNEngine.h"


namespace RXNEngine {

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
		Ref<Framebuffer> m_Framebuffer;
		Ref<Shader> m_ModelShader;
		Ref<Model> m_Model;
		Ref<Model> m_Model1;
		Ref<Model> m_CubeModel;
		Ref<EditorCamera> m_Camera;
		Ref<TextureCube> m_Skybox;
		LightEnvironment m_Lights;

		Scene m_Scene;
		Entity m_CameraEntity;
		Entity m_ModelEntity;
		Entity m_SkyboxEntity;

		float m_FPS;
	};

	class CameraControllerScript : public ScriptableEntity
	{
	public:
		void OnCreate()
		{
			// Setup initial speed, etc.
		}

		void OnUpdate(float ts)
		{
			auto& translation = GetComponent<TransformComponent>().Translation;
			float speed = 5.0f * ts;

			if (Input::IsKeyPressed(KeyCode::W))
				translation.z -= speed;
			if (Input::IsKeyPressed(KeyCode::A))
				translation.x -= speed;
			if (Input::IsKeyPressed(KeyCode::S))
				translation.z += speed;
			if (Input::IsKeyPressed(KeyCode::D))
				translation.x += speed;
			if (Input::IsKeyPressed(KeyCode::Space))
				translation.y += speed;
			if (Input::IsKeyPressed(KeyCode::LeftShift))
				translation.y -= speed;
		}
	};
}