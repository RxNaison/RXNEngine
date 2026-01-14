#pragma once
#include "RXNEngine.h"
#include "RXNEngine/Renderer/Model.h"


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
		Ref<Model> m_CubeModel;
		Ref<EditorCamera> m_Camera;
		Ref<TextureCube> m_Skybox;
		LightEnvironment m_Lights;

		float m_FPS;
	};

}