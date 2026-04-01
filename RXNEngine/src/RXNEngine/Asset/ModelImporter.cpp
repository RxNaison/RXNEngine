#include "rxnpch.h"
#include "ModelImporter.h"

#include "RXNEngine/Asset/AssetManager.h"
#include "RXNEngine/Core/Log.h"
#include "RXNEngine/Scene/Components.h"
#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Serialization/ModelSerializer.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <filesystem>
#include <fstream>

namespace RXNEngine {

	static glm::mat4 AssimpToGLM(const aiMatrix4x4& from)
	{
		glm::mat4 to;
		to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
		to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
		to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
		to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
		return to;
	}

	static Ref<Texture2D> LoadMaterialTexture(aiMaterial* mat, aiTextureType type, const aiScene* scene, const std::string& directory, const std::string& modelPath)
	{
		if (mat->GetTextureCount(type) > 0)
		{
			aiString str;
			mat->GetTexture(type, 0, &str);

			const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(str.C_Str());

			auto assetManager = Application::Get().GetSubsystem<AssetManager>();

			if (embeddedTexture)
			{
				std::filesystem::path mPath = modelPath;
				std::string modelName = mPath.stem().string();

				std::string texIdentifier = str.C_Str();
				std::replace(texIdentifier.begin(), texIdentifier.end(), '*', '_');

				std::string extractedPath = directory + "/" + modelName + "_tex" + texIdentifier + ".png";

				if (!std::filesystem::exists(extractedPath))
				{
					if (embeddedTexture->mHeight == 0)
					{
						std::ofstream out(extractedPath, std::ios::binary);
						out.write((const char*)embeddedTexture->pcData, embeddedTexture->mWidth);
						out.close();
						RXN_CORE_INFO("Extracted embedded texture to: {0}", extractedPath);
					}
					else
					{
						RXN_CORE_WARN("Uncompressed embedded textures not supported for extraction!");
						return nullptr;
					}
				}

				return assetManager->GetTexture(extractedPath);
			}
			else
			{
				std::string filename = std::string(str.C_Str());
				std::replace(filename.begin(), filename.end(), '\\', '/');
				std::string filepath = directory + "/" + filename;

				return assetManager->GetTexture(filepath);
			}
		}
		return nullptr;
	}

	static Ref<Texture2D> LoadTextureWithFallback(aiMaterial* mat, const std::vector<aiTextureType>& types, const aiScene* scene, const std::string& directory, const std::string& modelPath)
	{
		for (auto type : types)
		{
			if (mat->GetTextureCount(type) > 0)
				return LoadMaterialTexture(mat, type, scene, directory, modelPath);
		}
		return nullptr;
	}

	static std::string GetMaterialTexturePath(aiMaterial* mat, const std::vector<aiTextureType>& types, const aiScene* scene, const std::string& directory)
	{
		for (auto type : types)
		{
			if (mat->GetTextureCount(type) > 0)
			{
				aiString str;
				mat->GetTexture(type, 0, &str);

				std::string filename = std::string(str.C_Str());
				std::replace(filename.begin(), filename.end(), '\\', '/');
				return directory + "/" + filename;
			}
		}
		return "";
	}

	Ref<StaticMesh> ModelImporter::ImportAsset(const std::string& filepath, const ModelImportSettings& settings)
	{
		std::string cachePath = filepath + ".rxn";

		if (!settings.ForceRebuildCache && std::filesystem::exists(cachePath))
		{
			RXN_CORE_INFO("Loading Cached Model: {0}", cachePath);
			Ref<StaticMesh> cachedMesh = ModelSerializer::Deserialize(cachePath);
			if (cachedMesh)
				return cachedMesh;
		}

		RXN_CORE_INFO("Importing raw model: {0}", filepath);

		ImporterData data;
		if (!LoadModelData(filepath, data, settings))
			return nullptr;

		Ref<StaticMesh> mesh = BuildMeshFromData(data);

		ModelSerializer::Serialize(cachePath, mesh);

		return mesh;
	}

	void ModelImporter::ProcessNode(aiNode* node, const aiScene* scene, ImporterData& data, const glm::mat4& parentTransform, const std::string& parentNodeName)
	{
		glm::mat4 localTransform = parentTransform * AssimpToGLM(node->mTransformation);
		std::string nodeName = node->mName.C_Str();

		if (nodeName.empty() || nodeName.find("$") != std::string::npos || nodeName.find("Object_") != std::string::npos)
		{
			nodeName = parentNodeName;
		}

		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			ProcessMesh(mesh, scene, data, nodeName, localTransform);
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
		{
			ProcessNode(node->mChildren[i], scene, data, localTransform, nodeName);
		}
	}

	void ModelImporter::ProcessMesh(aiMesh* mesh, const aiScene* scene, ImporterData& data, const std::string& nodeName, const glm::mat4& localTransform)
	{
		Submesh submesh;
		submesh.NodeName = nodeName;
		submesh.LocalTransform = localTransform;
		submesh.MaterialIndex = mesh->mMaterialIndex;
		submesh.BaseVertex = (uint32_t)data.Vertices.size();
		submesh.BaseIndex = (uint32_t)data.Indices.size();
		submesh.VertexCount = mesh->mNumVertices;
		submesh.IndexCount = mesh->mNumFaces * 3;

		glm::vec3 minAABB(FLT_MAX);
		glm::vec3 maxAABB(-FLT_MAX);

		for (uint32_t i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex;
			vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

			if (mesh->HasNormals())
				vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
			else
				vertex.Normal = { 0.0f, 0.0f, 0.0f };

			if (mesh->mTextureCoords[0])
				vertex.TexCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
			else
				vertex.TexCoord = { 0.0f, 0.0f };

			minAABB = glm::min(minAABB, vertex.Position);
			maxAABB = glm::max(maxAABB, vertex.Position);

			data.Vertices.push_back(vertex);
		}

		submesh.BoundingBox = { minAABB, maxAABB };

		for (uint32_t i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			for (uint32_t j = 0; j < face.mNumIndices; j++)
				data.Indices.push_back(face.mIndices[j] + submesh.BaseVertex);
		}

		data.Submeshes.push_back(submesh);
	}

	Entity ModelImporter::InstantiateToScene(Ref<Scene> scene, const std::string& filepath, const ModelImportSettings& settings)
	{
		Ref<StaticMesh> mesh = ImportAsset(filepath, settings);
		if (!mesh) return {};

		std::filesystem::path path(filepath);
		Entity rootEntity = scene->CreateEntity(path.stem().string());

		const auto& submeshes = mesh->GetSubmeshes();
		for (uint32_t i = 0; i < submeshes.size(); i++)
		{
			Entity child = scene->CreateEntity(submeshes[i].NodeName);
			scene->ParentEntity(child, rootEntity);

			glm::vec3 translation, rotation, scale;
			Math::DecomposeTransform(submeshes[i].LocalTransform, translation, rotation, scale);

			auto& tc = child.GetComponent<TransformComponent>();
			tc.Translation = translation;
			tc.Rotation = rotation;
			tc.Scale = scale;

			auto& mc = child.AddComponent<StaticMeshComponent>();
			mc.AssetPath = filepath;
			mc.Mesh = mesh;
			mc.SubmeshIndex = i;
		}

		return rootEntity;
	}

	bool ModelImporter::LoadModelData(const std::string& filepath, ImporterData& outData, const ModelImportSettings& settings)
	{
		OPTICK_EVENT();

		uint32_t importFlags = aiProcess_Triangulate;
		if (settings.GenerateNormals)
			importFlags |= aiProcess_GenSmoothNormals;
		if (settings.CalculateTangents)
			importFlags |= aiProcess_CalcTangentSpace;
		if (settings.JoinIdenticalVertices)
			importFlags |= aiProcess_JoinIdenticalVertices;
		if (settings.FlipUVs)
			importFlags |= aiProcess_FlipUVs;
		if (settings.OptimizeGraph)
			importFlags |= aiProcess_OptimizeGraph;
		if (settings.OptimizeMeshes)
			importFlags |= aiProcess_OptimizeMeshes;

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(filepath, importFlags);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
			return false;

		std::string directory = filepath.substr(0, filepath.find_last_of('/'));
		if (directory == filepath) directory = filepath.substr(0, filepath.find_last_of('\\'));

		for (uint32_t i = 0; i < scene->mNumMaterials; i++)
		{
			aiMaterial* aiMat = scene->mMaterials[i];
			MaterialDesc desc;

			desc.AlbedoPath = GetMaterialTexturePath(aiMat, { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }, scene, directory);
			desc.NormalPath = GetMaterialTexturePath(aiMat, { aiTextureType_NORMALS, aiTextureType_HEIGHT }, scene, directory);
			desc.MetalRoughPath = GetMaterialTexturePath(aiMat, { aiTextureType_METALNESS, aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_SPECULAR, aiTextureType_SHININESS }, scene, directory);
			desc.AOPath = GetMaterialTexturePath(aiMat, { aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP, aiTextureType_AMBIENT }, scene, directory);
			desc.EmissivePath = GetMaterialTexturePath(aiMat, { aiTextureType_EMISSIVE }, scene, directory);

			aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);
			if (aiMat->Get(AI_MATKEY_BASE_COLOR, color) != AI_SUCCESS)
				aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
			desc.AlbedoColor = { color.r, color.g, color.b, color.a };

			aiColor4D emissiveColor(0.0f, 0.0f, 0.0f, 1.0f);
			if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS)
				desc.EmissiveColor = { emissiveColor.r, emissiveColor.g, emissiveColor.b };

			float roughness = 0.5f;
			float metalness = 0.0f;
			if (!desc.MetalRoughPath.empty()) metalness = 1.0f;

			if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) != AI_SUCCESS)
			{
				float shininess;
				if (aiMat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
					roughness = 1.0f - (glm::sqrt(shininess) / 10.0f);
			}
			desc.Roughness = roughness;

			if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metalness) != AI_SUCCESS)
				metalness = 0.0f;
			desc.Metalness = metalness;

			float opacity = 1.0f;
			if (aiMat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS)
			{
				if (opacity < 1.0f || color.a < 1.0f)
					desc.Transparent = true;
			}

			aiString alphaMode;
			if (aiMat->Get("$mat.gltf.alphaMode", 0, 0, alphaMode) == AI_SUCCESS)
			{
				std::string mode = alphaMode.C_Str();
				if (mode == "BLEND" || mode == "MASK")
					desc.Transparent = true;
			}
			else
			{
				if (opacity < 1.0f || color.a < 1.0f)
					desc.Transparent = true;
			}

			outData.Materials.push_back(desc);
		}

		ProcessNode(scene->mRootNode, scene, outData, glm::mat4(1.0f), "Root");
		return true;
	}

	Ref<StaticMesh> ModelImporter::BuildMeshFromData(const ImporterData& data)
	{
		OPTICK_EVENT("ModelImporter::BuildMeshFromData (GPU)");

		auto assetManager = Application::Get().GetSubsystem<AssetManager>();

		Ref<Shader> defaultPBR = assetManager->GetShader("res/shaders/pbr.glsl");
		std::vector<Ref<Material>> finalMaterials;

		for (const auto& desc : data.Materials)
		{
			Ref<Material> rxnMat = Material::CreateDefault(defaultPBR);

			if (!desc.AlbedoPath.empty())
				rxnMat->SetAlbedoMap(assetManager->GetTexture(desc.AlbedoPath));

			if (!desc.NormalPath.empty())
				rxnMat->SetNormalMap(assetManager->GetTexture(desc.NormalPath));

			if (!desc.MetalRoughPath.empty())
				rxnMat->SetMetalnessRoughnessMap(assetManager->GetTexture(desc.MetalRoughPath));

			if (!desc.AOPath.empty())
				rxnMat->SetAOMap(assetManager->GetTexture(desc.AOPath));

			if (!desc.EmissivePath.empty())
				rxnMat->SetEmissiveMap(assetManager->GetTexture(desc.EmissivePath));

			rxnMat->SetAlbedoColor(desc.AlbedoColor);
			rxnMat->SetEmissiveColor(desc.EmissiveColor);
			rxnMat->SetRoughness(desc.Roughness);
			rxnMat->SetMetalness(desc.Metalness);
			rxnMat->SetTransparent(desc.Transparent);

			finalMaterials.push_back(rxnMat);
		}

		return CreateRef<StaticMesh>(data.Vertices, data.Indices, data.Submeshes, finalMaterials);
	}
}