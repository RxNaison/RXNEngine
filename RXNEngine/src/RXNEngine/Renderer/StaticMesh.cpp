#include "rxnpch.h"
#include "StaticMesh.h"
#include "RXNEngine/Renderer/Buffer.h"

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

		Ref<IndexBuffer> ibo = IndexBuffer::Create((uint32_t*)indices.data(), indices.size());
		m_VAO->SetIndexBuffer(ibo);
	}

}