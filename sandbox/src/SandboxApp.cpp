#include <RXNEngine.h>

class ExampleLayer : public RXNEngine::Layer
{
public:
	ExampleLayer()
		: Layer("Example")
	{
		// 1. --- Define a 3D Cube with Vertex Colors ---
		m_VertexArray.reset(RXNEngine::VertexArray::Create());

		// 8 vertices, each with 3 position floats and 4 color floats
		float vertices[] = {
			// Positions         // Colors
			-0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f, 1.0f, // 0
			 0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f, 1.0f, // 1
			 0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f, // 2
			-0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f, 1.0f, // 3
			-0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f, 1.0f, // 4
			 0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f, 1.0f, // 5
			 0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f, 1.0f, // 6
			-0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 0.0f, 1.0f  // 7
		};

		m_VertexBuffer.reset(RXNEngine::VertexBuffer::Create(vertices, sizeof(vertices)));

		// 2. --- Update the Buffer Layout ---
		RXNEngine::BufferLayout layout = {
			{ RXNEngine::ShaderDataType::Float3, "a_Position" },
			{ RXNEngine::ShaderDataType::Float4, "a_Color"    } // New color attribute
		};
		m_VertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(m_VertexBuffer);

		// 12 triangles, 3 indices per triangle = 36 indices
		uint32_t indices[] = {
			0, 1, 2, 2, 3, 0, // Front face
			4, 5, 1, 1, 0, 4, // Bottom face
			7, 6, 5, 5, 4, 7, // Back face
			3, 2, 6, 6, 7, 3, // Top face
			1, 5, 6, 6, 2, 1, // Right face
			4, 0, 3, 3, 7, 4  // Left face
		};
		m_IndexBuffer.reset(RXNEngine::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
		m_VertexArray->SetIndexBuffer(m_IndexBuffer);

		// 3. --- Update the Shaders for 3D ---
		std::string vertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec4 a_Color;

			uniform mat4 u_ViewProjection;
			uniform mat4 u_Model; // New model matrix for the object's transform

			out vec4 v_Color; // Pass color to fragment shader

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

		// 4. --- Set Camera to Perspective Mode ---
		m_Camera.SetPerspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
	}

	void OnUpdate() override // Assuming OnUpdate now provides a timestep
	{
		RXNEngine::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		RXNEngine::RenderCommand::Clear();

		// 4. --- Update Camera and Object Transforms ---

		// Move the camera back so we can see the cube at the origin
		m_Camera.SetPosition({ 1.5f, 0.0f, 5.0f });
		// Point the camera straight ahead (no rotation)
		m_Camera.SetOrientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

		RXNEngine::Renderer::BeginScene(m_Camera);

		// Make the cube spin!
		m_CubeRotationAngle += 0.1f;
		glm::mat4 modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::rotate(modelMatrix, glm::radians(m_CubeRotationAngle), glm::vec3(0.5f, 1.0f, 0.0f));

		RXNEngine::Renderer::Submit(m_Shader, m_VertexArray, modelMatrix);

		RXNEngine::Renderer::EndScene();
	}

private:
	std::shared_ptr<RXNEngine::Shader> m_Shader;
	std::shared_ptr<RXNEngine::VertexArray> m_VertexArray;
	std::shared_ptr<RXNEngine::VertexBuffer> m_VertexBuffer;
	std::shared_ptr<RXNEngine::IndexBuffer> m_IndexBuffer;
	RXNEngine::Camera m_Camera;

	float m_CubeRotationAngle = 0.0f; // To animate the cube
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