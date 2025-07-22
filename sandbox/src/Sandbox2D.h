#pragma once
#include <RXNEngine.h>

class Sandbox2D : public RXNEngine::Layer
{
public:
	Sandbox2D();
	virtual ~Sandbox2D() = default;

	virtual void OnAttach() override;
	virtual void OnDetach() override;
	virtual void OnUpdate(float deltaTime) override;
	virtual void OnFixedUpdate(float fixedDeltaTime) override;
	virtual void OnEvent(RXNEngine::Event& event) override;
	virtual void OnImGuiRenderer() override;
private:
	RXNEngine::Ref<RXNEngine::Shader> m_Shader;
	RXNEngine::Ref<RXNEngine::VertexArray> m_VertexArray;
	RXNEngine::Ref<RXNEngine::VertexBuffer> m_VertexBuffer;
	RXNEngine::Ref<RXNEngine::IndexBuffer> m_IndexBuffer;
	RXNEngine::Camera m_Camera;
	RXNEngine::CameraController m_CameraController;
	RXNEngine::Ref<RXNEngine::Texture2D> m_Texture;

};