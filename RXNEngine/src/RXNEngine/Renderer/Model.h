#pragma once

#include "Mesh.h"
#include "Material.h"
#include <vector>
#include <string>

#include <assimp/scene.h>

namespace RXNEngine {

	struct ModelSubmesh
	{
		Ref<Mesh> Geometry;
		Ref<Material> Surface;
		glm::mat4 LocalTransform;
		AABB BoundingBox;
	};

	class Model
	{
	public:
		Model(const std::string& path, const Ref<Shader>& defaultShader);
		~Model() = default;

		void OnRender(const glm::mat4& transform);

		const std::vector<ModelSubmesh>& GetSubmeshes() const { return m_Submeshes; }

	private:
		void ProcessNode(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform);
		void ProcessMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& transform);

		Ref<Texture2D> LoadMaterialTexture(aiMaterial* mat, aiTextureType type);

	private:
		std::vector<ModelSubmesh> m_Submeshes;
		std::string m_Directory;
		Ref<Shader> m_DefaultShader;
		std::unordered_map<std::string, Ref<Texture2D>> m_TextureCache;
	};
}