#include "Sandbox2D.h"


Sandbox2D::Sandbox2D()
	:Layer("Sandbox2D"), m_CameraController(&m_Camera)
{
}

void Sandbox2D::OnAttach()
{
	m_VertexArray = RXNEngine::VertexArray::Create();

	float squareVertices[5 * 4] = {
		-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
		 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
		 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
		-0.5f,  0.5f, 0.0f, 0.0f, 1.0f
	};

	RXNEngine::Ref<RXNEngine::VertexBuffer> squareVB = RXNEngine::VertexBuffer::Create(squareVertices, sizeof(squareVertices));
	squareVB->SetLayout({
		{ RXNEngine::ShaderDataType::Float3, "a_Position" },
		{ RXNEngine::ShaderDataType::Float2, "a_TexCoord" }
		});
	m_VertexArray->AddVertexBuffer(squareVB);

	uint32_t squareIndices[6] = { 0, 1, 2, 2, 3, 0 };
	RXNEngine::Ref<RXNEngine::IndexBuffer> squareIB = RXNEngine::IndexBuffer::Create(squareIndices, sizeof(squareIndices) / sizeof(uint32_t));
	m_VertexArray->SetIndexBuffer(squareIB);

	m_Texture = RXNEngine::Texture2D::Create("assets/textures/icon.png");
	m_Shader = RXNEngine::Shader::Create("assets/shaders/Texture2D.glsl");

	m_Shader->SetInt("u_Texture", 0);

	m_Camera.SetOrthographic(1.0f, 1280.0f / 720.0f, -1.0f, 1.0f);
	//m_Camera.SetPosition({ 0.0f, 0.0f, 0.0f });
	//m_Camera.SetOrientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

	m_CameraController.SetControlMode(RXNEngine::CameraController::ControlMode::Mode2D);
}

void Sandbox2D::OnDetach()
{
}

void Sandbox2D::OnUpdate(float deltaTime)
{
	m_CameraController.OnUpdate(deltaTime);

	RXNEngine::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	RXNEngine::RenderCommand::Clear();

	RXNEngine::Renderer::BeginScene(m_Camera);
	m_Texture->Bind();
	RXNEngine::Renderer::Submit(m_Shader, m_VertexArray, glm::scale(glm::mat4(1.0f), glm::vec3(0.5f)));
	RXNEngine::Renderer::EndScene();
}

void Sandbox2D::OnFixedUpdate(float fixedDeltaTime)
{
}

void Sandbox2D::OnEvent(RXNEngine::Event& event)
{
	m_CameraController.OnEvent(event);
}

void Sandbox2D::OnImGuiRenderer()
{
}
