#include "rxnpch.h"
#include "ModelImporter.h"

#include "RXNEngine/Asset/AssetManager.h"
#include "RXNEngine/Core/Log.h"
#include "RXNEngine/Scene/Components.h"
#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Serialization/ModelSerializer.h"
#include "RXNEngine/Serialization/MaterialSerializer.h"
#include "RXNEngine/Utils/PlatformUtils.h"
#include "RXNEngine/Core/Application.h"
#include "RXNEngine/Core/VFSSystem.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <filesystem>
#include <fstream>
#include <coacd.h>
#include <unordered_set>
#include <cmath>


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

	static glm::mat4 SafeInverse(const glm::mat4& m)
	{
		float det = glm::determinant(m);
		if (std::abs(det) < 1e-6f)
		{
			return glm::mat4(1.0f);
		}
		return glm::inverse(m);
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

				return assetManager->GetTexture(FileSystem::GetRelativePath(extractedPath));
			}
			else
			{
				std::string filename = std::string(str.C_Str());
				std::replace(filename.begin(), filename.end(), '\\', '/');
				std::string filepath = directory + "/" + filename;

				return assetManager->GetTexture(FileSystem::GetRelativePath(filepath));
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

	static std::string GetMaterialTexturePath(aiMaterial* mat, const std::vector<aiTextureType>& types, const aiScene* scene, const std::string& directory, const std::string& modelPath)
	{
		for (auto type : types)
		{
			if (mat->GetTextureCount(type) > 0)
			{
				aiString str;
				mat->GetTexture(type, 0, &str);

				const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(str.C_Str());
				if (embeddedTexture)
				{
					std::filesystem::path mPath = modelPath;
					std::string modelName = mPath.stem().string();

					std::string texIdentifier = str.C_Str();
					std::replace(texIdentifier.begin(), texIdentifier.end(), '*', '_');

					std::string ext = "png";
					if (embeddedTexture->achFormatHint[0] != '\0')
					{
						std::string hint = embeddedTexture->achFormatHint;
						if (hint[0] == '.')
							ext = hint.substr(1);
						else
							ext = hint;
					}

					std::string extractedPath = directory + "/" + modelName + "_tex" + texIdentifier + "." + ext;

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
							return "";
						}
					}

					return FileSystem::GetRelativePath(extractedPath);
				}
				else
				{
					std::string filename = std::string(str.C_Str());
					std::replace(filename.begin(), filename.end(), '\\', '/');
					return FileSystem::GetRelativePath(directory + "/" + filename);
				}
			}
		}
		return "";
	}

	Ref<StaticMesh> ModelImporter::ImportAsset(const std::string& filepath, const ModelImportSettings& settings)
	{
		std::string cachePath = filepath + ".rxn";

		auto vfs = Application::Get().GetSubsystem<VFSSystem>();
		bool cacheExists = (vfs && vfs->FileExists(cachePath)) || std::filesystem::exists(cachePath);

		if (!settings.ForceRebuildCache && cacheExists)
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

		Ref<StaticMesh> mesh = BuildMeshFromData(data, filepath);

		ModelSerializer::Serialize(cachePath, mesh);

		return mesh;
	}

	void ModelImporter::ProcessNode(aiNode* node, const aiScene* scene, ImporterData& data,
		const glm::mat4& parentTransform, const std::string& parentNodeName, const ModelImportSettings& settings)
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
			ProcessMesh(mesh, scene, data, nodeName, localTransform, settings);
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
		{
			ProcessNode(node->mChildren[i], scene, data, localTransform, nodeName, settings);
		}
	}

	void ModelImporter::ProcessMesh(aiMesh* mesh, const aiScene* scene, ImporterData& data,
		const std::string& nodeName, const glm::mat4& localTransform, const ModelImportSettings& settings)
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

		if (settings.GenerateCoACDHulls)
		{
			try
			{
				CoACD_Mesh coacd_mesh;

				std::vector<double> flat_vertices;
				flat_vertices.reserve(mesh->mNumVertices * 3);
				for (uint32_t i = 0; i < mesh->mNumVertices; i++)
				{
					flat_vertices.push_back(mesh->mVertices[i].x);
					flat_vertices.push_back(mesh->mVertices[i].y);
					flat_vertices.push_back(mesh->mVertices[i].z);
				}
				coacd_mesh.vertices_ptr = flat_vertices.data();
				coacd_mesh.vertices_count = mesh->mNumVertices;

				std::vector<int> flat_indices;
				flat_indices.reserve(mesh->mNumFaces * 3);
				for (uint32_t i = 0; i < mesh->mNumFaces; i++) 
				{
					aiFace face = mesh->mFaces[i];
					if (face.mNumIndices == 3)
					{
						flat_indices.push_back(face.mIndices[0]);
						flat_indices.push_back(face.mIndices[1]);
						flat_indices.push_back(face.mIndices[2]);
					}
				}
				coacd_mesh.triangles_ptr = flat_indices.data();
				coacd_mesh.triangles_count = flat_indices.size() / 3;

				CoACD_setLogLevel("error");

				CoACD_MeshArray arr = CoACD_run(
					coacd_mesh,
					settings.CoACD_Threshold,      // threshold
					settings.CoACD_MaxHulls,       // max_convex_hull
					0,                             // preprocess_mode (auto)
					settings.CoACD_PrepResolution, // prep_resolution
					2000,                          // sample_resolution
					settings.CoACD_MctsNodes,      // mcts_nodes
					settings.CoACD_MctsIterations, // mcts_iteration
					settings.CoACD_MctsMaxDepth,   // mcts_max_depth
					false,  // pca
					true,   // merge
					false,  // decimate
					256,    // max_ch_vertex
					false,  // extrude
					0.01,   // extrude_margin
					0,      // apx_mode (0 = Convex Hull)
					0,      // seed
					false   // real_metric
				);

				for (uint64_t i = 0; i < arr.meshes_count; ++i)
				{
					ConvexHullData hullData;
					CoACD_Mesh& m = arr.meshes_ptr[i];

					for (uint64_t v = 0; v < m.vertices_count; ++v)
					{
						hullData.Vertices.push_back({
							(float)m.vertices_ptr[3 * v],
							(float)m.vertices_ptr[3 * v + 1],
							(float)m.vertices_ptr[3 * v + 2]
							});
					}

					for (uint64_t f = 0; f < m.triangles_count; ++f)
					{
						hullData.Indices.push_back((uint32_t)m.triangles_ptr[3 * f]);
						hullData.Indices.push_back((uint32_t)m.triangles_ptr[3 * f + 1]);
						hullData.Indices.push_back((uint32_t)m.triangles_ptr[3 * f + 2]);
					}
					submesh.ConvexHulls.push_back(hullData);
				}

				CoACD_freeMeshArray(arr);

				RXN_CORE_INFO("CoACD: Shattered '{0}' into {1} convex hulls (Manifold Auto-Repair Applied).", nodeName, submesh.ConvexHulls.size());
			}
			catch (const std::exception& e)
			{
				RXN_CORE_WARN("CoACD failed for mesh '{0}': {1}", nodeName, e.what());
				RXN_CORE_WARN("Falling back to standard Convex Hull generation.");
			}
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
		RXN_PROFILE_SCOPE();

		uint32_t importFlags = aiProcess_Triangulate;
		if (settings.GenerateNormals)
			importFlags |= aiProcess_GenSmoothNormals;
		if (settings.CalculateTangents)
			importFlags |= aiProcess_CalcTangentSpace;
		if (settings.JoinIdenticalVertices)
			importFlags |= aiProcess_JoinIdenticalVertices;
		if (settings.FlipUVs)
			importFlags |= aiProcess_FlipUVs;

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(filepath, importFlags);

		if (!scene || !scene->mRootNode)
			return false;

		bool hasBones = false;
		for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
		{
			if (scene->mMeshes[i]->mNumBones > 0)
			{
				hasBones = true;
				break;
			}
		}

		if (!hasBones)
		{
			uint32_t optFlags = 0;
			if (settings.OptimizeGraph)
				optFlags |= aiProcess_OptimizeGraph;
			if (settings.OptimizeMeshes)
				optFlags |= aiProcess_OptimizeMeshes;

			if (optFlags != 0)
			{
				scene = importer.ApplyPostProcessing(optFlags);
				if (!scene || !scene->mRootNode)
					return false;
			}
		}

		std::string directory = filepath.substr(0, filepath.find_last_of('/'));
		if (directory == filepath) directory = filepath.substr(0, filepath.find_last_of('\\'));

		for (uint32_t i = 0; i < scene->mNumMaterials; i++)
		{
			aiMaterial* aiMat = scene->mMaterials[i];
			MaterialDesc desc;

			aiString matName;
			if (aiMat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS)
				desc.Name = matName.C_Str();
			else
				desc.Name = "Material_" + std::to_string(i);

			std::replace(desc.Name.begin(), desc.Name.end(), '/', '_');
			std::replace(desc.Name.begin(), desc.Name.end(), '\\', '_');
			std::replace(desc.Name.begin(), desc.Name.end(), ':', '_');

			desc.AlbedoPath = GetMaterialTexturePath(aiMat, { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }, scene, directory, filepath);
			desc.NormalPath = GetMaterialTexturePath(aiMat, { aiTextureType_NORMALS, aiTextureType_HEIGHT }, scene, directory, filepath);
			desc.MetalPath = GetMaterialTexturePath(aiMat, { aiTextureType_METALNESS, aiTextureType_SPECULAR, aiTextureType_SHININESS }, scene, directory, filepath);
			desc.RoughPath = GetMaterialTexturePath(aiMat, { aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_SPECULAR, aiTextureType_SHININESS }, scene, directory, filepath);
			desc.AOPath = GetMaterialTexturePath(aiMat, { aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP, aiTextureType_AMBIENT }, scene, directory, filepath);
			desc.EmissivePath = GetMaterialTexturePath(aiMat, { aiTextureType_EMISSIVE }, scene, directory, filepath);

			aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);
			if (aiMat->Get(AI_MATKEY_BASE_COLOR, color) != AI_SUCCESS)
				aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
			desc.AlbedoColor = { color.r, color.g, color.b, color.a };

			aiColor4D emissiveColor(0.0f, 0.0f, 0.0f, 1.0f);
			if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS)
				desc.EmissiveColor = { emissiveColor.r, emissiveColor.g, emissiveColor.b };

			float roughness = 0.5f;
			float metalness = 0.0f;

			if (!desc.MetalPath.empty())
				metalness = 1.0f;

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

		ProcessNode(scene->mRootNode, scene, outData, glm::mat4(1.0f), "Root", settings);
		return true;
	}

	template<typename T>
	static std::vector<uint32_t> GenerateLODIndices(const std::vector<T>& vertices, const std::vector<uint32_t>& indices,
		uint32_t baseVertex, uint32_t vertexCount, uint32_t baseIndex, uint32_t indexCount, const AABB& bbox, int gridResolution)
	{
		if (gridResolution <= 0 || indexCount == 0)
			return {};

		glm::vec3 size = bbox.Max - bbox.Min;
		if (size.x < 1e-4f) size.x = 1e-4f;
		if (size.y < 1e-4f) size.y = 1e-4f;
		if (size.z < 1e-4f) size.z = 1e-4f;

		std::unordered_map<uint64_t, uint32_t> cellToRepresentative;
		std::vector<uint32_t> vertexRemap(vertices.size());
		for (uint32_t i = 0; i < vertices.size(); ++i)
		{
			vertexRemap[i] = i;
		}

		for (uint32_t i = baseVertex; i < baseVertex + vertexCount && i < vertices.size(); ++i)
		{
			const auto& v = vertices[i];
			glm::vec3 normPos = (v.Position - bbox.Min) / size;
			int cx = glm::clamp(static_cast<int>(normPos.x * gridResolution), 0, gridResolution - 1);
			int cy = glm::clamp(static_cast<int>(normPos.y * gridResolution), 0, gridResolution - 1);
			int cz = glm::clamp(static_cast<int>(normPos.z * gridResolution), 0, gridResolution - 1);

			uint64_t cellKey = (static_cast<uint64_t>(cx) << 40) | (static_cast<uint64_t>(cy) << 20) | static_cast<uint64_t>(cz);
			
			auto it = cellToRepresentative.find(cellKey);
			if (it == cellToRepresentative.end())
			{
				cellToRepresentative[cellKey] = i;
			}
			else
			{
				vertexRemap[i] = it->second;
			}
		}

		std::vector<uint32_t> lodIndices;
		lodIndices.reserve(indexCount);

		for (uint32_t i = 0; i < indexCount; i += 3)
		{
			uint32_t i0 = indices[baseIndex + i];
			uint32_t i1 = indices[baseIndex + i + 1];
			uint32_t i2 = indices[baseIndex + i + 2];

			if (i0 >= vertexRemap.size() || i1 >= vertexRemap.size() || i2 >= vertexRemap.size())
			{
				lodIndices.push_back(i0);
				lodIndices.push_back(i1);
				lodIndices.push_back(i2);
				continue;
			}

			uint32_t r0 = vertexRemap[i0];
			uint32_t r1 = vertexRemap[i1];
			uint32_t r2 = vertexRemap[i2];

			if (r0 != r1 && r1 != r2 && r2 != r0)
			{
				lodIndices.push_back(r0);
				lodIndices.push_back(r1);
				lodIndices.push_back(r2);
			}
		}

		if (lodIndices.empty())
		{
			for (uint32_t i = 0; i < indexCount; ++i)
			{
				lodIndices.push_back(indices[baseIndex + i]);
			}
		}

		return lodIndices;
	}

	Ref<StaticMesh> ModelImporter::BuildMeshFromData(const ImporterData& data, const std::string& modelFilepath)
	{
		RXN_PROFILE_SCOPE();

		auto assetManager = Application::Get().GetSubsystem<AssetManager>();
		Ref<Shader> defaultPBR = assetManager->GetShader("res/shaders/pbr.glsl");
		std::vector<Ref<Material>> finalMaterials;

		std::string directory = modelFilepath.substr(0, modelFilepath.find_last_of('/'));
		if (directory == modelFilepath)
			directory = modelFilepath.substr(0, modelFilepath.find_last_of('\\'));

		for (const auto& desc : data.Materials)
		{
			std::string matPath = directory + "/" + desc.Name + ".rxnmat";
			matPath = FileSystem::GetRelativePath(matPath);

			if (std::filesystem::exists(matPath))
			{
				finalMaterials.push_back(assetManager->GetMaterial(matPath));
			}
			else
			{
				Ref<Material> rxnMat = Material::CreateDefault(defaultPBR);
				MaterialParameters rxnMatParams = rxnMat->GetParameters();

				if (!desc.AlbedoPath.empty())
					rxnMat->SetAlbedoMap(assetManager->GetTexture(desc.AlbedoPath), desc.AlbedoPath);

				if (!desc.NormalPath.empty())
					rxnMat->SetNormalMap(assetManager->GetTexture(desc.NormalPath, TextureUsage::NormalMap), desc.NormalPath);

				if (!desc.MetalPath.empty())
					rxnMat->SetMetalnessMap(assetManager->GetTexture(desc.MetalPath), desc.MetalPath);

				if (!desc.RoughPath.empty())
					rxnMat->SetRoughnessMap(assetManager->GetTexture(desc.RoughPath), desc.RoughPath);

				if (!desc.AOPath.empty())
					rxnMat->SetAOMap(assetManager->GetTexture(desc.AOPath), desc.AOPath);

				if (!desc.EmissivePath.empty())
					rxnMat->SetEmissiveMap(assetManager->GetTexture(desc.EmissivePath), desc.EmissivePath);

				rxnMatParams.AlbedoColor = desc.AlbedoColor;
				rxnMatParams.EmissiveColor = desc.EmissiveColor;
				rxnMatParams.Roughness = desc.Roughness;
				rxnMatParams.Metalness = desc.Metalness;
				rxnMat->SetTransparent(desc.Transparent);

				rxnMat->SetParameters(rxnMatParams);

				MaterialSerializer serializer(rxnMat);
				serializer.Serialize(matPath);

				finalMaterials.push_back(assetManager->GetMaterial(matPath));
			}
		}

		std::vector<uint32_t> finalIndices = data.Indices;
		std::vector<Submesh> finalSubmeshes = data.Submeshes;

		for (auto& submesh : finalSubmeshes)
		{
			submesh.LODs.clear();
			submesh.LODs.push_back({ submesh.BaseIndex, submesh.IndexCount });

			std::vector<uint32_t> lod1 = GenerateLODIndices(data.Vertices, data.Indices, submesh.BaseVertex, submesh.VertexCount, submesh.BaseIndex, submesh.IndexCount, submesh.BoundingBox, 64);
			uint32_t lod1Base = (uint32_t)finalIndices.size();
			finalIndices.insert(finalIndices.end(), lod1.begin(), lod1.end());
			submesh.LODs.push_back({ lod1Base, (uint32_t)lod1.size() });

			std::vector<uint32_t> lod2 = GenerateLODIndices(data.Vertices, data.Indices, submesh.BaseVertex, submesh.VertexCount, submesh.BaseIndex, submesh.IndexCount, submesh.BoundingBox, 32);
			uint32_t lod2Base = (uint32_t)finalIndices.size();
			finalIndices.insert(finalIndices.end(), lod2.begin(), lod2.end());
			submesh.LODs.push_back({ lod2Base, (uint32_t)lod2.size() });
		}

		return CreateRef<StaticMesh>(data.Vertices, finalIndices, finalSubmeshes, finalMaterials);
	}


	struct TempBoneInfo
	{
		int32_t Index = -1;
		glm::mat4 InverseBindMatrix = glm::mat4(1.0f);
	};

	static void ExtractBonesPrePass(aiNode* node, const aiScene* scene, std::unordered_map<std::string, TempBoneInfo>& boneMapping, int32_t& boneCount)
	{
		std::unordered_set<std::string> activeNodes;
		for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
		{
			aiMesh* mesh = scene->mMeshes[m];
			for (uint32_t b = 0; b < mesh->mNumBones; ++b)
			{
				activeNodes.insert(mesh->mBones[b]->mName.C_Str());
			}
		}

		for (uint32_t a = 0; a < scene->mNumAnimations; ++a)
		{
			aiAnimation* aiAnim = scene->mAnimations[a];
			for (uint32_t c = 0; c < aiAnim->mNumChannels; ++c)
			{
				activeNodes.insert(aiAnim->mChannels[c]->mNodeName.C_Str());
			}
		}

		struct Helper {
			static bool Build(aiNode* n, const aiScene* sc, const std::unordered_set<std::string>& activeSet,
				std::unordered_map<std::string, TempBoneInfo>& mapping, int32_t& count, const glm::mat4& parentXform)
			{
				std::string name = n->mName.C_Str();
				glm::mat4 xform = parentXform * AssimpToGLM(n->mTransformation);

				bool active = (activeSet.find(name) != activeSet.end()) || (n->mNumMeshes > 0);

				for (uint32_t i = 0; i < n->mNumChildren; ++i)
				{
					if (Build(n->mChildren[i], sc, activeSet, mapping, count, xform))
						active = true;
				}

				if (active)
				{
					if (mapping.find(name) == mapping.end())
					{
						TempBoneInfo info;
						info.Index = count++;

						bool foundExplicitOffset = false;
						for (uint32_t m = 0; m < sc->mNumMeshes; ++m)
						{
							aiMesh* mesh = sc->mMeshes[m];
							for (uint32_t b = 0; b < mesh->mNumBones; ++b)
							{
								if (std::string(mesh->mBones[b]->mName.C_Str()) == name)
								{
									info.InverseBindMatrix = AssimpToGLM(mesh->mBones[b]->mOffsetMatrix);
									foundExplicitOffset = true;
									break;
								}
							}
							if (foundExplicitOffset) break;
						}

						if (!foundExplicitOffset)
						{
							info.InverseBindMatrix = SafeInverse(xform);
						}

						mapping[name] = info;
					}
				}

				return active;
			}
		};

		Helper::Build(scene->mRootNode, scene, activeNodes, boneMapping, boneCount, glm::mat4(1.0f));
	}

	static bool HasBonesInSubtree(aiNode* node, const std::unordered_map<std::string, TempBoneInfo>& boneMapping)
	{
		if (boneMapping.find(node->mName.C_Str()) != boneMapping.end())
			return true;
		for (uint32_t i = 0; i < node->mNumChildren; ++i)
		{
			if (HasBonesInSubtree(node->mChildren[i], boneMapping))
				return true;
		}
		return false;
	}

	static void BuildSkeletonHierarchy(aiNode* node, int32_t parentIndex, const std::unordered_map<std::string, TempBoneInfo>& boneMapping, Ref<Skeleton> skeleton, const glm::mat4& parentTransform)
	{
		std::string nodeName = node->mName.C_Str();
		glm::mat4 nodeTransform = parentTransform * AssimpToGLM(node->mTransformation);
		
		int32_t currentJointIndex = parentIndex;

		if (HasBonesInSubtree(node, boneMapping))
		{
			auto it = boneMapping.find(nodeName);
			if (it != boneMapping.end())
			{
				skeleton->AddJoint(nodeName, parentIndex, it->second.InverseBindMatrix);
				currentJointIndex = static_cast<int32_t>(skeleton->GetJointCount() - 1);
			}
			else
			{
				glm::mat4 invBind = SafeInverse(nodeTransform);
				skeleton->AddJoint(nodeName, parentIndex, invBind);
				currentJointIndex = static_cast<int32_t>(skeleton->GetJointCount() - 1);
			}

			for (uint32_t i = 0; i < node->mNumChildren; ++i)
			{
				BuildSkeletonHierarchy(node->mChildren[i], currentJointIndex, boneMapping, skeleton, nodeTransform);
			}
		}
	}

	static glm::vec3 InterpolateTranslation(float time, aiNodeAnim* nodeAnim)
	{
		if (std::isnan(time) || std::isinf(time))
			time = 0.0f;

		if (!nodeAnim || nodeAnim->mNumPositionKeys == 0)
			return glm::vec3(0.0f);

		if (nodeAnim->mNumPositionKeys == 1)
		{
			return { nodeAnim->mPositionKeys[0].mValue.x, nodeAnim->mPositionKeys[0].mValue.y, nodeAnim->mPositionKeys[0].mValue.z };
		}

		uint32_t index = 0;
		for (uint32_t i = 0; i < nodeAnim->mNumPositionKeys - 1; ++i)
		{
			if (time < (float)nodeAnim->mPositionKeys[i + 1].mTime)
			{
				index = i;
				break;
			}
		}
		uint32_t nextIndex = index + 1;
		float t0 = (float)nodeAnim->mPositionKeys[index].mTime;
		float t1 = (float)nodeAnim->mPositionKeys[nextIndex].mTime;
		float factor = (t1 - t0 > 0.0001f) ? (time - t0) / (t1 - t0) : 0.0f;
		factor = glm::clamp(factor, 0.0f, 1.0f);

		aiVector3D start = nodeAnim->mPositionKeys[index].mValue;
		aiVector3D end = nodeAnim->mPositionKeys[nextIndex].mValue;
		aiVector3D result = start + (end - start) * factor;
		return { result.x, result.y, result.z };
	}

	static glm::quat InterpolateRotation(float time, aiNodeAnim* nodeAnim)
	{
		if (std::isnan(time) || std::isinf(time))
			time = 0.0f;

		if (!nodeAnim || nodeAnim->mNumRotationKeys == 0)
			return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

		if (nodeAnim->mNumRotationKeys == 1)
		{
			return { nodeAnim->mRotationKeys[0].mValue.w, nodeAnim->mRotationKeys[0].mValue.x, nodeAnim->mRotationKeys[0].mValue.y, nodeAnim->mRotationKeys[0].mValue.z };
		}

		uint32_t index = 0;
		for (uint32_t i = 0; i < nodeAnim->mNumRotationKeys - 1; ++i)
		{
			if (time < (float)nodeAnim->mRotationKeys[i + 1].mTime)
			{
				index = i;
				break;
			}
		}
		uint32_t nextIndex = index + 1;
		float t0 = (float)nodeAnim->mRotationKeys[index].mTime;
		float t1 = (float)nodeAnim->mRotationKeys[nextIndex].mTime;
		float factor = (t1 - t0 > 0.0001f) ? (time - t0) / (t1 - t0) : 0.0f;
		factor = glm::clamp(factor, 0.0f, 1.0f);

		aiQuaternion start = nodeAnim->mRotationKeys[index].mValue;
		aiQuaternion end = nodeAnim->mRotationKeys[nextIndex].mValue;
		aiQuaternion result;
		aiQuaternion::Interpolate(result, start, end, factor);
		return { result.w, result.x, result.y, result.z };
	}

	Ref<SkeletalMesh> ModelImporter::ImportSkeletalMesh(const std::string& filepath, const ModelImportSettings& settings)
	{
		std::string cachePath = filepath + ".rxnskel";

		auto vfs = Application::Get().GetSubsystem<VFSSystem>();
		bool cacheExists = (vfs && vfs->FileExists(cachePath)) || std::filesystem::exists(cachePath);

		if (!settings.ForceRebuildCache && cacheExists)
		{
			RXN_CORE_INFO("Loading Cached Skeletal Model: {0}", cachePath);
			Ref<SkeletalMesh> cachedMesh = ModelSerializer::DeserializeSkeletal(cachePath);
			if (cachedMesh)
				return cachedMesh;
		}

		RXN_CORE_INFO("Importing raw skeletal model: {0}", filepath);

		ImporterData data;
		if (!LoadSkeletalData(filepath, data, settings))
			return nullptr;

		Ref<SkeletalMesh> mesh = BuildSkeletalMeshFromData(data, filepath);
		if (mesh)
		{
			ModelSerializer::SerializeSkeletal(cachePath, mesh);
		}
		return mesh;
	}

	void ModelImporter::ProcessSkeletalNode(aiNode* node, const aiScene* scene, ImporterData& data,
		const glm::mat4& parentTransform, const std::string& parentNodeName, const ModelImportSettings& settings)
	{
		glm::mat4 localTransform = parentTransform * AssimpToGLM(node->mTransformation);
		std::string nodeName = node->mName.C_Str();

		if (nodeName.empty())
		{
			nodeName = parentNodeName;
		}

		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			ProcessSkeletalMesh(mesh, scene, data, nodeName, localTransform, settings);
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
		{
			ProcessSkeletalNode(node->mChildren[i], scene, data, localTransform, nodeName, settings);
		}
	}

	void ModelImporter::ProcessSkeletalMesh(aiMesh* mesh, const aiScene* scene, ImporterData& data,
		const std::string& nodeName, const glm::mat4& localTransform, const ModelImportSettings& settings)
	{
		Submesh submesh;
		submesh.NodeName = nodeName;
		submesh.LocalTransform = localTransform;
		submesh.MaterialIndex = mesh->mMaterialIndex;
		submesh.BaseVertex = (uint32_t)data.SkinnedVertices.size();
		submesh.BaseIndex = (uint32_t)data.Indices.size();
		submesh.VertexCount = mesh->mNumVertices;
		submesh.IndexCount = mesh->mNumFaces * 3;

		glm::vec3 minAABB(FLT_MAX);
		glm::vec3 maxAABB(-FLT_MAX);

		for (uint32_t i = 0; i < mesh->mNumVertices; i++)
		{
			SkinnedVertex vertex;
			vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

			if (mesh->HasNormals())
				vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

			if (mesh->mNumBones == 0)
			{
				glm::vec4 posRoot = localTransform * glm::vec4(vertex.Position, 1.0f);
				vertex.Position = glm::vec3(posRoot);

				if (mesh->HasNormals())
				{
					float det = glm::determinant(glm::mat3(localTransform));
					if (glm::abs(det) > 1e-6f)
					{
						glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(localTransform)));
						vertex.Normal = glm::normalize(normalMatrix * vertex.Normal);
					}
					else
					{
						vertex.Normal = glm::normalize(glm::mat3(localTransform) * vertex.Normal);
					}
				}
			}

			if (mesh->mTextureCoords[0])
				vertex.TexCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };

			minAABB = glm::min(minAABB, vertex.Position);
			maxAABB = glm::max(maxAABB, vertex.Position);

			data.SkinnedVertices.push_back(vertex);
		}

		submesh.BoundingBox = { minAABB, maxAABB };

		for (uint32_t i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			for (uint32_t j = 0; j < face.mNumIndices; j++)
				data.Indices.push_back(face.mIndices[j] + submesh.BaseVertex);
		}

		if (data.MeshSkeleton)
		{
			if (mesh->mNumBones > 0)
			{
				for (uint32_t b = 0; b < mesh->mNumBones; ++b)
				{
					aiBone* bone = mesh->mBones[b];
					std::string boneName = bone->mName.C_Str();
					int32_t jointIndex = data.MeshSkeleton->FindJointIndex(boneName);

					if (jointIndex >= 0)
					{
						for (uint32_t w = 0; w < bone->mNumWeights; ++w)
						{
							aiVertexWeight vw = bone->mWeights[w];
							uint32_t localVertexId = vw.mVertexId;
							uint32_t globalVertexId = submesh.BaseVertex + localVertexId;

							if (globalVertexId < data.SkinnedVertices.size())
							{
								data.SkinnedVertices[globalVertexId].AddBoneInfluence(jointIndex, vw.mWeight);
							}
						}
					}
				}
			}
			else
			{
				int32_t jointIndex = data.MeshSkeleton->FindJointIndex(nodeName);
				if (jointIndex >= 0)
				{
					for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
					{
						uint32_t globalVertexId = submesh.BaseVertex + i;
						if (globalVertexId < data.SkinnedVertices.size())
						{
							data.SkinnedVertices[globalVertexId].AddBoneInfluence(jointIndex, 1.0f);
						}
					}
				}
			}

			for (uint32_t i = submesh.BaseVertex; i < submesh.BaseVertex + submesh.VertexCount; ++i)
			{
				data.SkinnedVertices[i].NormalizeWeights();
			}
		}

		data.Submeshes.push_back(submesh);
	}

	static glm::mat4 GetLocalBindPoseMatrix(const Ref<Skeleton>& skeleton, uint32_t jointIndex)
	{
		const auto& invBindPose = skeleton->GetInverseBindPose();
		int32_t parent = skeleton->GetParentIndices()[jointIndex];
		glm::mat4 globalBind = SafeInverse(invBindPose[jointIndex]);
		if (parent < 0)
		{
			return globalBind;
		}
		else
		{
			return invBindPose[parent] * globalBind;
		}
	}

	bool ModelImporter::LoadSkeletalData(const std::string& filepath, ImporterData& outData, const ModelImportSettings& settings)
	{
		RXN_PROFILE_SCOPE();

		uint32_t importFlags = aiProcess_Triangulate | aiProcess_LimitBoneWeights;
		if (settings.GenerateNormals)
			importFlags |= aiProcess_GenSmoothNormals;
		if (settings.CalculateTangents)
			importFlags |= aiProcess_CalcTangentSpace;
		if (settings.JoinIdenticalVertices)
			importFlags |= aiProcess_JoinIdenticalVertices;
		if (settings.FlipUVs)
			importFlags |= aiProcess_FlipUVs;

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(filepath, importFlags);

		if (!scene || !scene->mRootNode)
		{
			std::string errorStr = importer.GetErrorString();
			RXN_CORE_ERROR("Assimp failed to load skeletal file: {0}. Error: {1}", filepath, errorStr);
			return false;
		}

		std::string directory = filepath.substr(0, filepath.find_last_of('/'));
		if (directory == filepath)
			directory = filepath.substr(0, filepath.find_last_of('\\'));

		for (uint32_t i = 0; i < scene->mNumMaterials; i++)
		{
			aiMaterial* aiMat = scene->mMaterials[i];
			MaterialDesc desc;

			aiString matName;
			if (aiMat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS)
				desc.Name = matName.C_Str();
			else
				desc.Name = "Material_" + std::to_string(i);

			std::replace(desc.Name.begin(), desc.Name.end(), '/', '_');
			std::replace(desc.Name.begin(), desc.Name.end(), '\\', '_');
			std::replace(desc.Name.begin(), desc.Name.end(), ':', '_');

			desc.AlbedoPath = GetMaterialTexturePath(aiMat, { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }, scene, directory, filepath);
			desc.NormalPath = GetMaterialTexturePath(aiMat, { aiTextureType_NORMALS, aiTextureType_HEIGHT }, scene, directory, filepath);
			desc.MetalPath = GetMaterialTexturePath(aiMat, { aiTextureType_METALNESS, aiTextureType_SPECULAR, aiTextureType_SHININESS }, scene, directory, filepath);
			desc.RoughPath = GetMaterialTexturePath(aiMat, { aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_SPECULAR, aiTextureType_SHININESS }, scene, directory, filepath);
			desc.AOPath = GetMaterialTexturePath(aiMat, { aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP, aiTextureType_AMBIENT }, scene, directory, filepath);
			desc.EmissivePath = GetMaterialTexturePath(aiMat, { aiTextureType_EMISSIVE }, scene, directory, filepath);

			aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);
			if (aiMat->Get(AI_MATKEY_BASE_COLOR, color) != AI_SUCCESS)
				aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
			desc.AlbedoColor = { color.r, color.g, color.b, color.a };

			aiColor4D emissiveColor(0.0f, 0.0f, 0.0f, 1.0f);
			if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS)
				desc.EmissiveColor = { emissiveColor.r, emissiveColor.g, emissiveColor.b };

			float roughness = 0.5f;
			float metalness = 0.0f;

			if (!desc.MetalPath.empty())
				metalness = 1.0f;

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
			if (aiMat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS && opacity < 1.0f)
				desc.Transparent = true;

			outData.Materials.push_back(desc);
		}

		std::unordered_map<std::string, TempBoneInfo> boneMapping;
		int32_t boneCount = 0;
		ExtractBonesPrePass(scene->mRootNode, scene, boneMapping, boneCount);

		if (boneCount > 0)
		{
			outData.MeshSkeleton = CreateRef<Skeleton>();
			BuildSkeletonHierarchy(scene->mRootNode, -1, boneMapping, outData.MeshSkeleton, glm::mat4(1.0f));
			outData.MeshSkeleton->Finalize();
		}

		ProcessSkeletalNode(scene->mRootNode, scene, outData, glm::mat4(1.0f), "Root", settings);

		if (scene->HasAnimations() && outData.MeshSkeleton)
		{
			uint32_t jointCount = outData.MeshSkeleton->GetJointCount();
			for (uint32_t a = 0; a < scene->mNumAnimations; ++a)
			{
				aiAnimation* aiAnim = scene->mAnimations[a];
				
				auto clip = CreateRef<AnimationClip>();
				clip->SetName(aiAnim->mName.C_Str());
				const float targetTPS = static_cast<float>(aiAnim->mTicksPerSecond > 0.0 ? aiAnim->mTicksPerSecond : 30.0);
				float duration = static_cast<float>(aiAnim->mDuration / targetTPS);
				if (std::isnan(duration) || std::isinf(duration) || duration < 0.0f || duration > 3600.0f)
				{
					duration = 1.0f;
				}
				clip->SetDuration(duration);

				uint32_t frameCount = static_cast<uint32_t>(duration * targetTPS);
				if (frameCount == 0) frameCount = 1;
				if (frameCount > 100000) frameCount = 100000;

				clip->Resize(frameCount, jointCount);
				clip->SetTicksPerSecond(targetTPS);
				clip->InitializeRawTracks(jointCount);
				clip->SetSourceModelAsset(filepath);

				std::unordered_map<std::string, aiNodeAnim*> channelMap;
				for (uint32_t c = 0; c < aiAnim->mNumChannels; ++c)
				{
					channelMap[aiAnim->mChannels[c]->mNodeName.C_Str()] = aiAnim->mChannels[c];
				}

				const std::vector<std::string>& jointNames = outData.MeshSkeleton->GetJointNames();
				clip->SetJointNames(jointNames);

				for (uint32_t j = 0; j < jointCount; ++j)
				{
					std::string jointName = jointNames[j];
					std::vector<glm::quat> resampledRotations(frameCount, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
					std::vector<glm::vec3> resampledTranslations(frameCount, glm::vec3(0.0f));

					glm::vec3 translationMin(FLT_MAX);
					glm::vec3 translationMax(-FLT_MAX);
					glm::vec3 jointScale(1.0f);

					auto& rawTrack = clip->GetRawTracks()[j];
					rawTrack.JointName = jointName;

					auto it = channelMap.find(jointName);
					if (it != channelMap.end())
					{
						aiNodeAnim* channel = it->second;

						float tpsVal = static_cast<float>(aiAnim->mTicksPerSecond > 0.0 ? aiAnim->mTicksPerSecond : 24.0);

						for (uint32_t k = 0; k < channel->mNumPositionKeys; ++k)
						{
							TranslationKey key;
							key.Time = static_cast<float>(channel->mPositionKeys[k].mTime) / tpsVal;
							key.Value = { channel->mPositionKeys[k].mValue.x, channel->mPositionKeys[k].mValue.y, channel->mPositionKeys[k].mValue.z };
							key.Mode = InterpolationMode::Linear;
							rawTrack.TranslationKeys.push_back(key);
						}

						for (uint32_t k = 0; k < channel->mNumRotationKeys; ++k)
						{
							RotationKey key;
							key.Time = static_cast<float>(channel->mRotationKeys[k].mTime) / tpsVal;
							key.Value = { channel->mRotationKeys[k].mValue.w, channel->mRotationKeys[k].mValue.x, channel->mRotationKeys[k].mValue.y, channel->mRotationKeys[k].mValue.z };
							key.Mode = InterpolationMode::Linear;
							rawTrack.RotationKeys.push_back(key);
						}

						for (uint32_t k = 0; k < channel->mNumScalingKeys; ++k)
						{
							ScaleKey key;
							key.Time = static_cast<float>(channel->mScalingKeys[k].mTime) / tpsVal;
							key.Value = { channel->mScalingKeys[k].mValue.x, channel->mScalingKeys[k].mValue.y, channel->mScalingKeys[k].mValue.z };
							key.Mode = InterpolationMode::Linear;
							rawTrack.ScaleKeys.push_back(key);
						}

						for (uint32_t f = 0; f < frameCount; ++f)
						{
							float timeInTicks = (static_cast<float>(f) / targetTPS) * static_cast<float>(aiAnim->mTicksPerSecond);
							
							resampledRotations[f] = InterpolateRotation(timeInTicks, channel);
							resampledTranslations[f] = InterpolateTranslation(timeInTicks, channel);

							translationMin = glm::min(translationMin, resampledTranslations[f]);
							translationMax = glm::max(translationMax, resampledTranslations[f]);
						}

						glm::mat4 localBind = GetLocalBindPoseMatrix(outData.MeshSkeleton, j);
						glm::vec3 translation = glm::vec3(0.0f);
						glm::vec3 rotationEuler = glm::vec3(0.0f);
						Math::DecomposeTransform(localBind, translation, rotationEuler, jointScale);
					}
					else
					{
						glm::mat4 localBind = GetLocalBindPoseMatrix(outData.MeshSkeleton, j);
						glm::vec3 translation = glm::vec3(0.0f);
						glm::vec3 rotationEuler = glm::vec3(0.0f);
						Math::DecomposeTransform(localBind, translation, rotationEuler, jointScale);
						glm::quat rotation = glm::quat(rotationEuler);

						for (uint32_t f = 0; f < frameCount; ++f)
						{
							resampledRotations[f] = rotation;
							resampledTranslations[f] = translation;
						}
						translationMin = translation;
						translationMax = translation;
					}

					if (translationMin.x > translationMax.x || translationMin.y > translationMax.y || translationMin.z > translationMax.z ||
						std::isnan(translationMin.x) || std::isnan(translationMin.y) || std::isnan(translationMin.z) ||
						std::isnan(translationMax.x) || std::isnan(translationMax.y) || std::isnan(translationMax.z))
					{
						translationMin = glm::vec3(0.0f);
						translationMax = glm::vec3(0.0f);
					}

					clip->SetTrackData(j, resampledRotations, resampledTranslations, translationMin, translationMax, jointScale);

					if (j == 0)
					{
						std::vector<float> distanceCurve(frameCount, 0.0f);
						float totalDistance = 0.0f;
						distanceCurve[0] = 0.0f;
						for (uint32_t f = 1; f < frameCount; ++f)
						{
							float dist = glm::distance(resampledTranslations[f], resampledTranslations[f - 1]);
							totalDistance += dist;
							distanceCurve[f] = totalDistance;
						}
						clip->SetDistanceCurve(distanceCurve, totalDistance);
					}
				}

				outData.Animations.push_back(clip);
				RXN_CORE_INFO("Imported and quantized clip: {0} ({1} frames, {2} joints resampled at {3} FPS).", clip->GetName(), frameCount, jointCount, targetTPS);
			}
		}

		return true;
	}

	Ref<SkeletalMesh> ModelImporter::BuildSkeletalMeshFromData(const ImporterData& data, const std::string& modelFilepath)
	{
		RXN_PROFILE_SCOPE();

		auto assetManager = Application::Get().GetSubsystem<AssetManager>();
		Ref<Shader> defaultPBR = assetManager->GetShader("res/shaders/pbr.glsl");
		std::vector<Ref<Material>> finalMaterials;

		std::string directory = modelFilepath.substr(0, modelFilepath.find_last_of('/'));
		if (directory == modelFilepath)
			directory = modelFilepath.substr(0, modelFilepath.find_last_of('\\'));

		for (const auto& desc : data.Materials)
		{
			std::string matPath = directory + "/" + desc.Name + ".rxnmat";
			matPath = FileSystem::GetRelativePath(matPath);

			if (std::filesystem::exists(matPath))
			{
				finalMaterials.push_back(assetManager->GetMaterial(matPath));
			}
			else
			{
				Ref<Material> rxnMat = Material::CreateDefault(defaultPBR);
				MaterialParameters rxnMatParams = rxnMat->GetParameters();

				if (!desc.AlbedoPath.empty())
					rxnMat->SetAlbedoMap(assetManager->GetTexture(desc.AlbedoPath), desc.AlbedoPath);

				if (!desc.NormalPath.empty())
					rxnMat->SetNormalMap(assetManager->GetTexture(desc.NormalPath, TextureUsage::NormalMap), desc.NormalPath);

				if (!desc.MetalPath.empty())
					rxnMat->SetMetalnessMap(assetManager->GetTexture(desc.MetalPath), desc.MetalPath);

				if (!desc.RoughPath.empty())
					rxnMat->SetRoughnessMap(assetManager->GetTexture(desc.RoughPath), desc.RoughPath);

				if (!desc.AOPath.empty())
					rxnMat->SetAOMap(assetManager->GetTexture(desc.AOPath), desc.AOPath);

				if (!desc.EmissivePath.empty())
					rxnMat->SetEmissiveMap(assetManager->GetTexture(desc.EmissivePath), desc.EmissivePath);

				rxnMatParams.AlbedoColor = desc.AlbedoColor;
				rxnMatParams.EmissiveColor = desc.EmissiveColor;
				rxnMatParams.Roughness = desc.Roughness;
				rxnMatParams.Metalness = desc.Metalness;
				rxnMat->SetTransparent(desc.Transparent);

				rxnMat->SetParameters(rxnMatParams);

				MaterialSerializer serializer(rxnMat);
				serializer.Serialize(matPath);

				finalMaterials.push_back(assetManager->GetMaterial(matPath));
			}
		}

		std::vector<uint32_t> finalIndices = data.Indices;
		std::vector<Submesh> finalSubmeshes = data.Submeshes;

		for (auto& submesh : finalSubmeshes)
		{
			submesh.LODs.clear();
			submesh.LODs.push_back({ submesh.BaseIndex, submesh.IndexCount });

			std::vector<uint32_t> lod1 = GenerateLODIndices(data.SkinnedVertices, data.Indices, submesh.BaseVertex, submesh.VertexCount, submesh.BaseIndex, submesh.IndexCount, submesh.BoundingBox, 64);
			uint32_t lod1Base = (uint32_t)finalIndices.size();
			finalIndices.insert(finalIndices.end(), lod1.begin(), lod1.end());
			submesh.LODs.push_back({ lod1Base, (uint32_t)lod1.size() });

			std::vector<uint32_t> lod2 = GenerateLODIndices(data.SkinnedVertices, data.Indices, submesh.BaseVertex, submesh.VertexCount, submesh.BaseIndex, submesh.IndexCount, submesh.BoundingBox, 32);
			uint32_t lod2Base = (uint32_t)finalIndices.size();
			finalIndices.insert(finalIndices.end(), lod2.begin(), lod2.end());
			submesh.LODs.push_back({ lod2Base, (uint32_t)lod2.size() });
		}

		return CreateRef<SkeletalMesh>(data.SkinnedVertices, finalIndices, finalSubmeshes, finalMaterials, data.MeshSkeleton);
	}
}