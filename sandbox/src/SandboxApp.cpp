#include <RXNEngine.h>

class ExampleLayer : public RXNEngine::Layer
{
public:
	ExampleLayer()
		: Layer("Example"), m_CameraController(&m_Camera)
	{
		m_VertexArray.reset(RXNEngine::VertexArray::Create());

		float vertices[] = {
			// Positions         // Colors
			-0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f, 1.0f,
			 0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f, 1.0f,
			 0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f,
			-0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f, 1.0f,
			-0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f, 1.0f,
			 0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f, 1.0f,
			 0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f, 1.0f,
			-0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 0.0f, 1.0f 
		};

		m_VertexBuffer.reset(RXNEngine::VertexBuffer::Create(vertices, sizeof(vertices)));

		RXNEngine::BufferLayout layout = {
			{ RXNEngine::ShaderDataType::Float3, "a_Position" },
			{ RXNEngine::ShaderDataType::Float4, "a_Color"    }
		};
		m_VertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(m_VertexBuffer);

		uint32_t indices[] = {
			0, 1, 2, 2, 3, 0,
			4, 5, 1, 1, 0, 4,
			7, 6, 5, 5, 4, 7,
			3, 2, 6, 6, 7, 3,
			1, 5, 6, 6, 2, 1,
			4, 0, 3, 3, 7, 4 
		};
		m_IndexBuffer.reset(RXNEngine::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
		m_VertexArray->SetIndexBuffer(m_IndexBuffer);

		m_Shader = m_ShaderLibrary.Load("assets/shaders/CubePosCol.glsl");

		m_Camera.SetPerspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
		m_Camera.SetPosition({ 0.0f, 0.0f, 5.0f });
		m_Camera.SetOrientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

		m_CameraController.SetControlMode(RXNEngine::CameraController::ControlMode::Mode3D);
	}

	void OnUpdate(float deltaTime) override
	{
		m_CameraController.OnUpdate(deltaTime);

		RXNEngine::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		RXNEngine::RenderCommand::Clear();

		RXNEngine::Renderer::BeginScene(m_Camera);

		m_CubeRotationAngle += 45.0f * deltaTime;
		glm::mat4 modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::rotate(modelMatrix, glm::radians(m_CubeRotationAngle), glm::vec3(0.5f, 1.0f, 0.0f));

		RXNEngine::Renderer::Submit(m_Shader, m_VertexArray, modelMatrix);

		RXNEngine::Renderer::EndScene();
	}

	void OnFixedUpdate(float fixedDeltaTime) override
	{
	}

	void OnEvent(RXNEngine::Event& event) override
	{
		m_CameraController.OnEvent(event);
	}

private:
	RXNEngine::ShaderLibrary m_ShaderLibrary;
	RXNEngine::Ref<RXNEngine::Shader> m_Shader;
	RXNEngine::Ref<RXNEngine::VertexArray> m_VertexArray;
	RXNEngine::Ref<RXNEngine::VertexBuffer> m_VertexBuffer;
	RXNEngine::Ref<RXNEngine::IndexBuffer> m_IndexBuffer;
	RXNEngine::Camera m_Camera;
	RXNEngine::CameraController m_CameraController;

	float m_CubeRotationAngle = 0.0f;
};

class Sandbox : public RXNEngine::Application
{
public:
	Sandbox()
	{
		PushLayer(new ExampleLayer);
	}
};

RXNEngine::Application* RXNEngine::CreateApplication() 
{
	return new Sandbox();
}