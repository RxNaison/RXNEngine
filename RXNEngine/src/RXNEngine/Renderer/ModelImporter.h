#pragma once

#include "StaticMesh.h"
#include "RXNEngine/Scene/Scene.h"

#include <assimp/scene.h>

namespace RXNEngine {

	struct ModelImportSettings
	{
		bool ForceRebuildCache = false;
		bool GenerateNormals = true;
		bool CalculateTangents = true;
		bool JoinIdenticalVertices = true;
		bool FlipUVs = false;
		bool OptimizeGraph = false; 
		bool OptimizeMeshes = false;
	};

	class ModelImporter
	{
	public:
		static Ref<StaticMesh> ImportAsset(const std::string& filepath, const ModelImportSettings& settings = ModelImportSettings());
		static Entity InstantiateToScene(Ref<Scene> scene, const std::string& filepath, const ModelImportSettings& settings = ModelImportSettings());
	private:
		struct ImporterData
		{
			std::vector<Vertex> Vertices;
			std::vector<uint32_t> Indices;
			std::vector<Submesh> Submeshes;
			std::vector<Ref<Material>> Materials;
		};

		static void ProcessNode(aiNode* node, const aiScene* scene, ImporterData& data, const glm::mat4& parentTransform, const std::string& parentNodeName);
		static void ProcessMesh(aiMesh* mesh, const aiScene* scene, ImporterData& data, const std::string& nodeName, const glm::mat4& localTransform);
	};

}