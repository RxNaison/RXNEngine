#include "rxnpch.h"
#include "Model.h"
#include "Renderer.h"
#include "RXNEngine/Core/Math.h"
#include "ModelSerializer.h"

#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

namespace RXNEngine {

	static glm::mat4 AssimpToGLM(const aiMatrix4x4& from)
	{
		glm::mat4 to;
		// Assimp is Row Major, GLM is Column Major
		to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
		to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
		to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
		to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
		return to;
	}

	Model::Model(const std::string& path, const Ref<Shader>& defaultShader)
		: m_DefaultShader(defaultShader), m_Path(path)
	{
		auto startTimepoint = std::chrono::steady_clock::now();

		std::string cachePath = path + ".rxn";

		if (std::filesystem::exists(cachePath))
		{
			RXN_CORE_INFO("Loading Cached Model: {0}", cachePath);
			if (ModelSerializer::Deserialize(cachePath, *this))
				return;
		}

		RXN_CORE_WARN("Cache miss. Importing raw model: {0}", path);

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path,
			aiProcess_Triangulate |
			aiProcess_GenSmoothNormals |
			//aiProcess_FlipUVs |
			aiProcess_CalcTangentSpace |
			aiProcess_OptimizeMeshes |
			aiProcess_OptimizeGraph |
			aiProcess_JoinIdenticalVertices |
			aiProcess_PreTransformVertices
		);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			RXN_CORE_ERROR("ERROR::ASSIMP:: {0}", importer.GetErrorString());
			return;
		}

		std::replace(m_Path.begin(), m_Path.end(), '\\', '/');
		m_Directory = m_Path.substr(0, m_Path.find_last_of('/'));

		ProcessNode(scene->mRootNode, scene, glm::mat4(1.0f));

		ModelSerializer::Serialize(cachePath, *this);
	}

	void Model::ProcessNode(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform)
	{
		glm::mat4 nodeTransform = parentTransform * AssimpToGLM(node->mTransformation);

		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			ProcessMesh(mesh, scene, nodeTransform);
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
		{
			ProcessNode(node->mChildren[i], scene, nodeTransform);
		}
	}

	void Model::ProcessMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& transform)
	{
		std::vector<float> vertices;
		std::vector<uint32_t> indices;

		glm::vec3 minAABB(FLT_MAX);
		glm::vec3 maxAABB(-FLT_MAX);

		// interleaved layout: Pos(3) + Normal(3) + TexCoord(2) = 8 floats per vertex
		for (uint32_t i = 0; i < mesh->mNumVertices; i++)
		{
			vertices.push_back(mesh->mVertices[i].x);
			vertices.push_back(mesh->mVertices[i].y);
			vertices.push_back(mesh->mVertices[i].z);

			minAABB.x = std::min(minAABB.x, mesh->mVertices[i].x);
			minAABB.y = std::min(minAABB.y, mesh->mVertices[i].y);
			minAABB.z = std::min(minAABB.z, mesh->mVertices[i].z);

			maxAABB.x = std::max(maxAABB.x, mesh->mVertices[i].x);
			maxAABB.y = std::max(maxAABB.y, mesh->mVertices[i].y);
			maxAABB.z = std::max(maxAABB.z, mesh->mVertices[i].z);

			if (mesh->HasNormals())
			{
				vertices.push_back(mesh->mNormals[i].x);
				vertices.push_back(mesh->mNormals[i].y);
				vertices.push_back(mesh->mNormals[i].z);
			}
			else
			{
				vertices.push_back(0.0f);
				vertices.push_back(0.0f);
				vertices.push_back(0.0f);
			}

			if (mesh->mTextureCoords[0])
			{
				vertices.push_back(mesh->mTextureCoords[0][i].x);
				vertices.push_back(mesh->mTextureCoords[0][i].y);
			}
			else
			{
				vertices.push_back(0.0f);
				vertices.push_back(0.0f);
			}
		}

		for (uint32_t i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			for (uint32_t j = 0; j < face.mNumIndices; j++)
				indices.push_back(face.mIndices[j]);
		}

		Ref<VertexArray> vao = VertexArray::Create();

		Ref<VertexBuffer> vbo = VertexBuffer::Create(vertices.data(), vertices.size() * sizeof(float));
		vbo->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Normal" },
			{ ShaderDataType::Float2, "a_TexCoord" }
			});
		vao->AddVertexBuffer(vbo);

		Ref<IndexBuffer> ibo = IndexBuffer::Create(indices.data(), indices.size());
		vao->SetIndexBuffer(ibo);

		Ref<Mesh> rxnMesh = CreateRef<Mesh>(vao);
		rxnMesh->SetAABB({ minAABB, maxAABB });

		rxnMesh->m_Vertices = vertices;
		rxnMesh->m_Indices = indices;

		Ref<Material> rxnMaterial = Material::CreateDefault(m_DefaultShader);

		if (mesh->mMaterialIndex >= 0)
		{
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

			Ref<Texture2D> albedoMap = LoadTextureWithFallback(material, { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }, scene);
			if (albedoMap)
				rxnMaterial->SetAlbedoMap(albedoMap);


			Ref<Texture2D> normalMap = LoadTextureWithFallback(material, { aiTextureType_NORMALS, aiTextureType_HEIGHT }, scene);
			if (normalMap)
				rxnMaterial->SetNormalMap(normalMap);


			Ref<Texture2D> metalRoughMap = LoadTextureWithFallback(material, { aiTextureType_METALNESS, aiTextureType_DIFFUSE_ROUGHNESS }, scene);
			if (!metalRoughMap)
				metalRoughMap = LoadTextureWithFallback(material, { aiTextureType_SPECULAR, aiTextureType_SHININESS }, scene);

			if (metalRoughMap) rxnMaterial->SetMetalnessRoughnessMap(metalRoughMap);


			Ref<Texture2D> aoMap = LoadTextureWithFallback(material, { aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP, aiTextureType_AMBIENT }, scene);
			if (aoMap) rxnMaterial->SetAOMap(aoMap);

			Ref<Texture2D> emissiveMap = LoadTextureWithFallback(material, { aiTextureType_EMISSIVE }, scene);
			if (emissiveMap) rxnMaterial->SetEmissiveMap(emissiveMap);

			aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);

			if (material->Get(AI_MATKEY_BASE_COLOR, color) != AI_SUCCESS)
				material->Get(AI_MATKEY_COLOR_DIFFUSE, color);

			rxnMaterial->SetAlbedoColor(glm::vec4(color.r, color.g, color.b, color.a));

			aiColor4D emissiveColor(0.0f, 0.0f, 0.0f, 1.0f);
			if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS)
				rxnMaterial->SetEmissiveColor(glm::vec3(emissiveColor.r, emissiveColor.g, emissiveColor.b));

			float roughness = 0.5f;
			float metalness = 0.0f;

			if (metalRoughMap) metalness = 1.0f;

			if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) != AI_SUCCESS)
			{
				float shininess;
				if (material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
					roughness = 1.0f - (glm::sqrt(shininess) / 10.0f);
			}
			rxnMaterial->SetRoughness(roughness);

			if (material->Get(AI_MATKEY_METALLIC_FACTOR, metalness) != AI_SUCCESS)
				metalness = 0.0f;
			
			rxnMaterial->SetMetalness(metalness);

			float opacity = 1.0f;
			if (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS)
			{
				if (opacity < 1.0f || color.a < 1.0f)
					rxnMaterial->SetTransparent(true);
			}
			aiString alphaMode;
			if (material->Get("$mat.gltf.alphaMode", 0, 0, alphaMode) == AI_SUCCESS)
			{
				std::string mode = alphaMode.C_Str();
				if (mode == "BLEND" || mode == "MASK")
				{
					rxnMaterial->SetTransparent(true);
				}
			}
			else
			{
				float opacity = 1.0f;
				material->Get(AI_MATKEY_OPACITY, opacity);

				aiColor4D color;
				material->Get(AI_MATKEY_BASE_COLOR, color);

				if (opacity < 1.0f || color.a < 1.0f)
				{
					rxnMaterial->SetTransparent(true);
				}
			}
		}
		else
		{
			rxnMaterial->SetAlbedoColor(glm::vec4(1.0f));
			rxnMaterial->SetRoughness(0.8f);
			rxnMaterial->SetMetalness(0.0f);
			rxnMaterial->SetAO(1.0f);
		}

		ModelSubmesh submesh;
		submesh.Geometry = rxnMesh;
		submesh.Surface = rxnMaterial;
		submesh.LocalTransform = transform;
		submesh.BoundingBox = { minAABB, maxAABB };

		m_Submeshes.push_back(submesh);
	}

	Ref<Texture2D> Model::LoadMaterialTexture(aiMaterial* mat, aiTextureType type, const aiScene* scene)
	{
		if (mat->GetTextureCount(type) > 0)
		{
			aiString str;
			mat->GetTexture(type, 0, &str);

			const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(str.C_Str());

			if (embeddedTexture)
			{
				if (embeddedTexture->mHeight == 0)
					return Texture2D::Create(embeddedTexture->pcData, embeddedTexture->mWidth);
				else
					RXN_CORE_WARN("Uncompressed embedded textures not yet implemented!");
			}
			else
			{
				std::string filename = std::string(str.C_Str());
				std::replace(filename.begin(), filename.end(), '\\', '/');

				std::string filepath = m_Directory + "/" + filename;

				if (m_TextureCache.find(filepath) != m_TextureCache.end())
				{
					return m_TextureCache[filepath];
				}

				Ref<Texture2D> newTexture = Texture2D::Create(filepath);

				if (newTexture->IsLoaded())
				{
					m_TextureCache[filepath] = newTexture;
					return newTexture;
				}
				else
				{
					RXN_CORE_WARN("Texture failed to load: {0}", filepath);
					return Texture2D::WhiteTexture();
				}
			}
		}
		return nullptr;
	}

	Ref<Texture2D> Model::LoadTextureWithFallback(aiMaterial* mat, const std::vector<aiTextureType>& types, const aiScene* scene)
	{
		for (auto type : types)
		{
			if (mat->GetTextureCount(type) > 0)
				return LoadMaterialTexture(mat, type, scene);
		}
		return nullptr;
	}
}