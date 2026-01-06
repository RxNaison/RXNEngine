#include "rxnpch.h"
#include "Model.h"
#include "Renderer.h"
#include "RXNEngine/Core/Math.h"

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
		: m_DefaultShader(defaultShader)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path,
			aiProcess_Triangulate |
			aiProcess_GenSmoothNormals |
			//aiProcess_FlipUVs |
			aiProcess_CalcTangentSpace |
			aiProcess_OptimizeMeshes |
			aiProcess_OptimizeGraph |
			aiProcess_JoinIdenticalVertices
		);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			RXN_CORE_ERROR("ERROR::ASSIMP:: {0}", importer.GetErrorString());
			return;
		}

		m_Directory = path.substr(0, path.find_last_of('/'));

		ProcessNode(scene->mRootNode, scene, glm::mat4(1.0f));
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

		Ref<Material> rxnMaterial = CreateRef<Material>(m_DefaultShader);

		if (mesh->mMaterialIndex >= 0)
		{
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

			Ref<Texture2D> albedoMap = LoadMaterialTexture(material, aiTextureType_BASE_COLOR, scene);

			if (!albedoMap)
				albedoMap = LoadMaterialTexture(material, aiTextureType_DIFFUSE, scene);

			if (albedoMap)
				rxnMaterial->SetTexture("u_AlbedoMap", albedoMap);
			else
				rxnMaterial->SetTexture("u_AlbedoMap", Texture2D::WhiteTexture());


			Ref<Texture2D> normalMap = LoadMaterialTexture(material, aiTextureType_NORMALS, scene);

			if (!normalMap)
				normalMap = LoadMaterialTexture(material, aiTextureType_HEIGHT, scene);

			if (normalMap)
			{
				rxnMaterial->SetTexture("u_NormalMap", normalMap);
				rxnMaterial->SetInt("u_UseNormalMap", 1);
			}
			else
			{
				rxnMaterial->SetTexture("u_NormalMap", Texture2D::BlueTexture());
				rxnMaterial->SetInt("u_UseNormalMap", 0);
			}

			Ref<Texture2D> metallicMap = LoadMaterialTexture(material, aiTextureType_METALNESS, scene);

			if (!metallicMap)
				metallicMap = LoadMaterialTexture(material, aiTextureType_SPECULAR, scene);

			if (metallicMap)
				rxnMaterial->SetTexture("u_MetallicMap", metallicMap);
			else
				rxnMaterial->SetTexture("u_MetallicMap", Texture2D::BlackTexture());


			Ref<Texture2D> roughnessMap = LoadMaterialTexture(material, aiTextureType_DIFFUSE_ROUGHNESS, scene);

			if (!roughnessMap)
				roughnessMap = LoadMaterialTexture(material, aiTextureType_SHININESS, scene);

			if (!roughnessMap)
				roughnessMap = LoadMaterialTexture(material, aiTextureType_SPECULAR, scene);

			if (roughnessMap)
				rxnMaterial->SetTexture("u_RoughnessMap", roughnessMap);
			else
				rxnMaterial->SetTexture("u_RoughnessMap", Texture2D::WhiteTexture());


			Ref<Texture2D> aoMap = LoadMaterialTexture(material, aiTextureType_AMBIENT_OCCLUSION, scene);
			if (!aoMap)
				aoMap = LoadMaterialTexture(material, aiTextureType_AMBIENT, scene);

			if (aoMap)
				rxnMaterial->SetTexture("u_AOMap", aoMap);
			else
				rxnMaterial->SetTexture("u_AOMap", Texture2D::WhiteTexture());

			rxnMaterial->SetFloat("u_Metallic", 1.0f);
			rxnMaterial->SetFloat("u_Roughness", 1.0f);
			rxnMaterial->SetFloat("u_AO", 1.0f);
			aiColor4D baseColor(1.0f, 1.0f, 1.0f, 1.0f);

			float floatVal;
			if (material->Get(AI_MATKEY_METALLIC_FACTOR, floatVal) == AI_SUCCESS)
				rxnMaterial->SetFloat("u_Metallic", floatVal);

			if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, floatVal) == AI_SUCCESS)
				rxnMaterial->SetFloat("u_Roughness", floatVal);

			if (material->Get(AI_MATKEY_BASE_COLOR, baseColor) != AI_SUCCESS)
				material->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor);

			rxnMaterial->SetFloat4("u_AlbedoColor", glm::vec4(baseColor.r, baseColor.g, baseColor.b, baseColor.a));

			Ref<Texture2D> opacityMap = LoadMaterialTexture(material, aiTextureType_OPACITY, scene);
			if (opacityMap)
			{
				rxnMaterial->SetTransparent(true);
			}

			float opacity = 1.0f;
			if (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS)
			{
				if (opacity < 1.0f)
					rxnMaterial->SetTransparent(true);
			}

			aiString alphaMode;
			if (material->Get("$mat.gltf.alphaMode", 0, 0, alphaMode) == AI_SUCCESS)
			{
				std::string mode = alphaMode.C_Str();

				if (mode == "BLEND")
				{
					rxnMaterial->SetTransparent(true);
				}
				else if (mode == "MASK")
				{
					rxnMaterial->SetTransparent(true);
					// read AI_MATKEY_GLTF_ALPHACUTOFF
				}
			}
		}
		else
		{
			rxnMaterial = std::make_shared<Material>(m_DefaultShader);
			rxnMaterial->SetTexture("u_AlbedoMap", Texture2D::WhiteTexture());
			rxnMaterial->SetTexture("u_NormalMap", Texture2D::WhiteTexture());
			rxnMaterial->SetTexture("u_MetallicMap", Texture2D::WhiteTexture());
			rxnMaterial->SetTexture("u_RoughnessMap", Texture2D::WhiteTexture());
			rxnMaterial->SetTexture("u_AOMap", Texture2D::WhiteTexture());

			rxnMaterial->SetFloat("u_Metallic", 0.0f);
			rxnMaterial->SetFloat("u_Roughness", 0.5f);
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
}