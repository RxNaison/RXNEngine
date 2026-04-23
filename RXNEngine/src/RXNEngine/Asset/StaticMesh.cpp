#include "rxnpch.h"
#include "StaticMesh.h"
#include "RXNEngine/Renderer/GraphicsAPI/Buffer.h"

namespace RXNEngine {

	StaticMesh::StaticMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices,
		const std::vector<Submesh>& submeshes, const std::vector<Ref<Material>>& materials)
		: m_Submeshes(submeshes), m_Materials(materials), m_Vertices(vertices), m_Indices(indices)
	{
		m_VAO = VertexArray::Create();

		Ref<VertexBuffer> vbo = VertexBuffer::Create((float*)vertices.data(), vertices.size() * sizeof(Vertex));
		vbo->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Normal" },
			{ ShaderDataType::Float2, "a_TexCoord" }
			});
		m_VAO->AddVertexBuffer(vbo);

		BufferLayout instanceLayout = {
			{ ShaderDataType::Float4, "a_ModelMatrixCol0", false, true },
			{ ShaderDataType::Float4, "a_ModelMatrixCol1", false, true },
			{ ShaderDataType::Float4, "a_ModelMatrixCol2", false, true },
			{ ShaderDataType::Float4, "a_ModelMatrixCol3", false, true },
			{ ShaderDataType::Int,    "a_EntityID",        false, true }
		};

		Ref<VertexBuffer> dummyInstanceVB = VertexBuffer::Create(sizeof(glm::mat4) + sizeof(int));
		dummyInstanceVB->SetLayout(instanceLayout);
		m_VAO->AddVertexBuffer(dummyInstanceVB);

		Ref<IndexBuffer> ibo = IndexBuffer::Create((uint32_t*)indices.data(), indices.size());
		m_VAO->SetIndexBuffer(ibo);
	}

}