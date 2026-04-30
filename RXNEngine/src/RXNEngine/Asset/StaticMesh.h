#pragma once

#include "RXNEngine/Renderer/GraphicsAPI/VertexArray.h"
#include "RXNEngine/Asset/Material.h"
#include "RXNEngine/Math/Math.h"

#include <vector>
#include <string>

namespace RXNEngine {

	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoord;
	};

	struct ConvexHullData
	{
		std::vector<glm::vec3> Vertices;
		std::vector<uint32_t> Indices;
	};

	struct Submesh
	{
		uint32_t BaseVertex;
		uint32_t BaseIndex;
		uint32_t MaterialIndex;
		uint32_t IndexCount;
		uint32_t VertexCount;
		AABB BoundingBox;
		std::string NodeName;
		glm::mat4 LocalTransform;
		std::vector<ConvexHullData> ConvexHulls;
	};

	class StaticMesh
	{
	public:
		StaticMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices,
			const std::vector<Submesh>& submeshes, const std::vector<Ref<Material>>& materials);
		~StaticMesh() = default;

		Ref<VertexArray> GetVertexArray() const { return m_VAO; }
		const std::vector<Submesh>& GetSubmeshes() const { return m_Submeshes; }
		const std::vector<Ref<Material>>& GetMaterials() const { return m_Materials; }
		const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
		const std::vector<uint32_t>& GetIndices() const { return m_Indices; }
	private:
		Ref<VertexArray> m_VAO;
		std::vector<Submesh> m_Submeshes;
		std::vector<Ref<Material>> m_Materials;

		std::vector<Vertex> m_Vertices;
		std::vector<uint32_t> m_Indices;
	};

}