#pragma once

#include "StaticMesh.h"
#include "RXNEngine/Scene/Scene.h"

#include <assimp/scene.h>

namespace RXNEngine {

	class ModelImporter
	{
	public:
		static Ref<StaticMesh> ImportAsset(const std::string& filepath);
		static Entity InstantiateToScene(Ref<Scene> scene, const std::string& filepath);
	private:
		struct ImporterData
		{
			std::vector<Vertex> Vertices;
			std::vector<uint32_t> Indices;
			std::vector<Submesh> Submeshes;
			std::vector<Ref<Material>> Materials;
		};

		static void ProcessNode(aiNode* node, const aiScene* scene, ImporterData& data, const glm::mat4& parentTransform);
		static void ProcessMesh(aiMesh* mesh, const aiScene* scene, ImporterData& data, const std::string& nodeName, const glm::mat4& localTransform);
	};

}