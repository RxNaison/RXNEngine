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

	struct MaterialDesc
	{
		std::string Name;
		std::string AlbedoPath, NormalPath, MetalPath, RoughPath, AOPath, EmissivePath;
		glm::vec4 AlbedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		glm::vec3 EmissiveColor = { 0.0f, 0.0f, 0.0f };
		float Roughness = 0.5f, Metalness = 0.0f;
		bool Transparent = false;
	};

	struct ImporterData
	{
		std::vector<Vertex> Vertices;
		std::vector<uint32_t> Indices;
		std::vector<Submesh> Submeshes;
		std::vector<MaterialDesc> Materials;
	};

	class ModelImporter
	{
	public:
		static Ref<StaticMesh> ImportAsset(const std::string& filepath, const ModelImportSettings& settings = ModelImportSettings());
		static Entity InstantiateToScene(Ref<Scene> scene, const std::string& filepath, const ModelImportSettings& settings = ModelImportSettings());

		static bool LoadModelData(const std::string& filepath, ImporterData& outData, const ModelImportSettings& settings = ModelImportSettings());
		static Ref<StaticMesh> BuildMeshFromData(const ImporterData& data, const std::string& modelFilepath);
	private:
		static void ProcessNode(aiNode* node, const aiScene* scene, ImporterData& data, const glm::mat4& parentTransform, const std::string& parentNodeName);
		static void ProcessMesh(aiMesh* mesh, const aiScene* scene, ImporterData& data, const std::string& nodeName, const glm::mat4& localTransform);
	};

}