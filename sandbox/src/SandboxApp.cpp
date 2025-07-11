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

		std::string vertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec4 a_Color;

			uniform mat4 u_ViewProjection;
			uniform mat4 u_Model;

			out vec4 v_Color;

			void main()
			{
				v_Color = a_Color;
				gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
			}
		)";

		std::string fragmentSrc = R"(
			#version 330 core
			
			layout(location = 0) out vec4 color;

			in vec4 v_Color; // Receive color from vertex shader

			void main()
			{
				color = v_Color;
			}
		)";

		m_Shader.reset(RXNEngine::Shader::Create(vertexSrc, fragmentSrc));

		m_Camera.SetPerspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
		m_Camera.SetPosition({ 1.5f, 0.0f, 5.0f });
		m_Camera.SetOrientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

		m_CameraController.SetControlMode(RXNEngine::CameraController::ControlMode::Mode2D);
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

private:
	std::shared_ptr<RXNEngine::Shader> m_Shader;
	std::shared_ptr<RXNEngine::VertexArray> m_VertexArray;
	std::shared_ptr<RXNEngine::VertexBuffer> m_VertexBuffer;
	std::shared_ptr<RXNEngine::IndexBuffer> m_IndexBuffer;
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