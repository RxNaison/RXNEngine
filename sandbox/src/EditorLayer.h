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
		Ref<RenderTarget> m_Framebuffer;
		//Ref<Scene> m_ActiveScene;
	};

}