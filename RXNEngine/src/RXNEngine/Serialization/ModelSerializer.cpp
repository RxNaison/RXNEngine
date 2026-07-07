#include "rxnpch.h"
#include "ModelSerializer.h"
#include "RXNEngine/Asset/AssetManager.h"
#include "RXNEngine/Utils/PlatformUtils.h"
#include "RXNEngine/Core/Application.h"
#include "RXNEngine/Core/VFSSystem.h"

#include <fstream>
#include <sstream>

namespace RXNEngine {

    static void WriteString(std::ofstream& out, const std::string& str)
    {
        uint32_t size = (uint32_t)str.size();
        out.write((char*)&size, sizeof(uint32_t));
        if (size > 0)
            out.write(str.c_str(), size);
    }

    static std::string ReadString(std::istream& in)
    {
        uint32_t size;
        in.read((char*)&size, sizeof(uint32_t));
        if (size > 0)
        {
            std::string str(size, '\0');
            in.read(&str[0], size);
            return str;
        }
        return "";
    }

    void ModelSerializer::Serialize(const std::string& filepath, const Ref<StaticMesh>& mesh)
    {
        std::ofstream out(filepath, std::ios::binary);
        if (!out.is_open())
        {
            RXN_CORE_ERROR("Failed to write binary asset: {0}", filepath);
            return;
        }

        const char* magic = "RXN\0";
        out.write(magic, 4);

        uint32_t version = 5;
        out.write((char*)&version, sizeof(uint32_t));

        const auto& vertices = mesh->GetVertices();
        const auto& indices = mesh->GetIndices();

        uint32_t vCount = (uint32_t)vertices.size();
        uint32_t iCount = (uint32_t)indices.size();
        out.write((char*)&vCount, sizeof(uint32_t));
        out.write((char*)&iCount, sizeof(uint32_t));

        out.write((char*)vertices.data(), vCount * sizeof(Vertex));
        out.write((char*)indices.data(), iCount * sizeof(uint32_t));

        const auto& submeshes = mesh->GetSubmeshes();
        uint32_t submeshCount = (uint32_t)submeshes.size();
        out.write((char*)&submeshCount, sizeof(uint32_t));

        for (const auto& submesh : submeshes)
        {
            out.write((char*)&submesh.BaseVertex, sizeof(uint32_t));
            out.write((char*)&submesh.BaseIndex, sizeof(uint32_t));
            out.write((char*)&submesh.MaterialIndex, sizeof(uint32_t));
            out.write((char*)&submesh.IndexCount, sizeof(uint32_t));
            out.write((char*)&submesh.VertexCount, sizeof(uint32_t));
            out.write((char*)&submesh.BoundingBox, sizeof(AABB));
            out.write((char*)&submesh.LocalTransform, sizeof(glm::mat4));
            WriteString(out, submesh.NodeName);

            uint32_t lodCount = (uint32_t)submesh.LODs.size();
            out.write((char*)&lodCount, sizeof(uint32_t));
            for (const auto& lod : submesh.LODs)
            {
                out.write((char*)&lod.BaseIndex, sizeof(uint32_t));
                out.write((char*)&lod.IndexCount, sizeof(uint32_t));
            }

            uint32_t hullCount = (uint32_t)submesh.ConvexHulls.size();
            out.write((char*)&hullCount, sizeof(uint32_t));

            for (const auto& hull : submesh.ConvexHulls)
            {
                uint32_t hvCount = (uint32_t)hull.Vertices.size();
                uint32_t hiCount = (uint32_t)hull.Indices.size();
                out.write((char*)&hvCount, sizeof(uint32_t));
                out.write((char*)&hiCount, sizeof(uint32_t));
                out.write((char*)hull.Vertices.data(), hvCount * sizeof(glm::vec3));
                out.write((char*)hull.Indices.data(), hiCount * sizeof(uint32_t));
            }
        }

        const auto& materials = mesh->GetMaterials();
        uint32_t matCount = (uint32_t)materials.size();
        out.write((char*)&matCount, sizeof(uint32_t));

        for (const auto& mat : materials)
            WriteString(out, FileSystem::GetRelativePath(mat->GetAssetPath()));

        out.close();
    }

    static Ref<StaticMesh> DeserializeStream(std::istream& in)
    {
        char magic[4];
        in.read(magic, 4);

        if (std::string(magic, 4) != std::string("RXN\0", 4))
            return nullptr;

        uint32_t version;
        in.read((char*)&version, sizeof(uint32_t));
        if (version != 4 && version != 5)
        {
            RXN_CORE_WARN("Old model format detected. It will be re-imported.");
            return nullptr;
        }

        uint32_t vCount = 0, iCount = 0;
        in.read((char*)&vCount, sizeof(uint32_t));
        in.read((char*)&iCount, sizeof(uint32_t));

        std::vector<Vertex> vertices(vCount);
        std::vector<uint32_t> indices(iCount);
        in.read((char*)vertices.data(), vCount * sizeof(Vertex));
        in.read((char*)indices.data(), iCount * sizeof(uint32_t));

        uint32_t submeshCount;
        in.read((char*)&submeshCount, sizeof(uint32_t));
        std::vector<Submesh> submeshes(submeshCount);

        for (uint32_t i = 0; i < submeshCount; i++)
        {
            in.read((char*)&submeshes[i].BaseVertex, sizeof(uint32_t));
            in.read((char*)&submeshes[i].BaseIndex, sizeof(uint32_t));
            in.read((char*)&submeshes[i].MaterialIndex, sizeof(uint32_t));
            in.read((char*)&submeshes[i].IndexCount, sizeof(uint32_t));
            in.read((char*)&submeshes[i].VertexCount, sizeof(uint32_t));
            in.read((char*)&submeshes[i].BoundingBox, sizeof(AABB));
            in.read((char*)&submeshes[i].LocalTransform, sizeof(glm::mat4));
            submeshes[i].NodeName = ReadString(in);

            if (version >= 5)
            {
                uint32_t lodCount = 0;
                in.read((char*)&lodCount, sizeof(uint32_t));
                submeshes[i].LODs.resize(lodCount);
                for (uint32_t l = 0; l < lodCount; ++l)
                {
                    in.read((char*)&submeshes[i].LODs[l].BaseIndex, sizeof(uint32_t));
                    in.read((char*)&submeshes[i].LODs[l].IndexCount, sizeof(uint32_t));
                }
            }
            else
            {
                submeshes[i].LODs.push_back({ submeshes[i].BaseIndex, submeshes[i].IndexCount });
            }

            uint32_t hullCount;
            in.read((char*)&hullCount, sizeof(uint32_t));
            submeshes[i].ConvexHulls.resize(hullCount);

            for (uint32_t h = 0; h < hullCount; h++)
            {
                uint32_t hvCount, hiCount;
                in.read((char*)&hvCount, sizeof(uint32_t));
                in.read((char*)&hiCount, sizeof(uint32_t));

                submeshes[i].ConvexHulls[h].Vertices.resize(hvCount);
                submeshes[i].ConvexHulls[h].Indices.resize(hiCount);

                in.read((char*)submeshes[i].ConvexHulls[h].Vertices.data(), hvCount * sizeof(glm::vec3));
                in.read((char*)submeshes[i].ConvexHulls[h].Indices.data(), hiCount * sizeof(uint32_t));
            }
        }

        uint32_t matCount;
        in.read((char*)&matCount, sizeof(uint32_t));
        std::vector<Ref<Material>> materials(matCount);

		auto assetManager = Application::Get().GetSubsystem<AssetManager>();

        for (uint32_t i = 0; i < matCount; i++)
        {
            std::string matPath = ReadString(in);

            if (!matPath.empty())
            {
                materials[i] = assetManager->GetMaterial(matPath);
            }
            else
            {
                Ref<Shader> defaultPBR = assetManager->GetShader("res/shaders/pbr.glsl");
                materials[i] = Material::CreateDefault(defaultPBR);
            }
        }

        return CreateRef<StaticMesh>(vertices, indices, submeshes, materials);
    }

    Ref<StaticMesh> ModelSerializer::Deserialize(const std::string& filepath)
    {
        auto vfs = Application::Get().GetSubsystem<VFSSystem>();
        if (vfs && vfs->FileExists(filepath))
        {
            auto fileData = vfs->ReadFile(filepath);
            std::string content((const char*)fileData.data(), fileData.size());
            std::istringstream in(content, std::ios::binary);
            return DeserializeStream(in);
        }
        else
        {
            std::ifstream in(filepath, std::ios::binary);
            if (!in.is_open())
                return nullptr;
            return DeserializeStream(in);
        }
    }

    void ModelSerializer::SerializeSkeletal(const std::string& filepath, const Ref<SkeletalMesh>& mesh)
    {
        std::ofstream out(filepath, std::ios::binary);
        if (!out.is_open())
        {
            RXN_CORE_ERROR("Failed to write binary skeletal asset: {0}", filepath);
            return;
        }

        const char* magic = "SKEL";
        out.write(magic, 4);

        uint32_t version = 3;
        out.write((char*)&version, sizeof(uint32_t));

        const auto& vertices = mesh->GetVertices();
        const auto& indices = mesh->GetIndices();

        uint32_t vCount = (uint32_t)vertices.size();
        uint32_t iCount = (uint32_t)indices.size();
        out.write((char*)&vCount, sizeof(uint32_t));
        out.write((char*)&iCount, sizeof(uint32_t));

        out.write((char*)vertices.data(), vCount * sizeof(SkinnedVertex));
        out.write((char*)indices.data(), iCount * sizeof(uint32_t));

        const auto& submeshes = mesh->GetSubmeshes();
        uint32_t submeshCount = (uint32_t)submeshes.size();
        out.write((char*)&submeshCount, sizeof(uint32_t));

        for (const auto& submesh : submeshes)
        {
            out.write((char*)&submesh.BaseVertex, sizeof(uint32_t));
            out.write((char*)&submesh.BaseIndex, sizeof(uint32_t));
            out.write((char*)&submesh.MaterialIndex, sizeof(uint32_t));
            out.write((char*)&submesh.IndexCount, sizeof(uint32_t));
            out.write((char*)&submesh.VertexCount, sizeof(uint32_t));
            out.write((char*)&submesh.BoundingBox, sizeof(AABB));
            out.write((char*)&submesh.LocalTransform, sizeof(glm::mat4));
            WriteString(out, submesh.NodeName);

            uint32_t lodCount = (uint32_t)submesh.LODs.size();
            out.write((char*)&lodCount, sizeof(uint32_t));
            for (const auto& lod : submesh.LODs)
            {
                out.write((char*)&lod.BaseIndex, sizeof(uint32_t));
                out.write((char*)&lod.IndexCount, sizeof(uint32_t));
            }

            uint32_t hullCount = (uint32_t)submesh.ConvexHulls.size();
            out.write((char*)&hullCount, sizeof(uint32_t));

            for (const auto& hull : submesh.ConvexHulls)
            {
                uint32_t hvCount = (uint32_t)hull.Vertices.size();
                uint32_t hiCount = (uint32_t)hull.Indices.size();
                out.write((char*)&hvCount, sizeof(uint32_t));
                out.write((char*)&hiCount, sizeof(uint32_t));
                out.write((char*)hull.Vertices.data(), hvCount * sizeof(glm::vec3));
                out.write((char*)hull.Indices.data(), hiCount * sizeof(uint32_t));
            }
        }

        const auto& materials = mesh->GetMaterials();
        uint32_t matCount = (uint32_t)materials.size();
        out.write((char*)&matCount, sizeof(uint32_t));

        for (const auto& mat : materials)
            WriteString(out, FileSystem::GetRelativePath(mat->GetAssetPath()));

        Ref<Skeleton> skeleton = mesh->GetSkeleton();
        bool hasSkeleton = (skeleton != nullptr);
        out.write((char*)&hasSkeleton, sizeof(bool));
        if (hasSkeleton)
        {
            uint32_t jointCount = skeleton->GetJointCount();
            out.write((char*)&jointCount, sizeof(uint32_t));

            const auto& parentIndices = skeleton->GetParentIndices();
            const auto& jointNames = skeleton->GetJointNames();
            const auto& invBindPoses = skeleton->GetInverseBindPose();

            for (uint32_t j = 0; j < jointCount; ++j)
            {
                WriteString(out, jointNames[j]);
                out.write((const char*)&parentIndices[j], sizeof(int32_t));
                out.write((const char*)&invBindPoses[j], sizeof(glm::mat4));
            }
        }

        out.close();
    }

    static Ref<SkeletalMesh> DeserializeSkeletalStream(std::istream& in)
    {
        char magic[4];
        in.read(magic, 4);

        if (std::string(magic, 4) != std::string("SKEL", 4))
            return nullptr;

        uint32_t version;
        in.read((char*)&version, sizeof(uint32_t));
        if (version != 1 && version != 2 && version != 3)
        {
            RXN_CORE_WARN("Unsupported skeletal model format version detected.");
            return nullptr;
        }

        uint32_t vCount = 0, iCount = 0;
        in.read((char*)&vCount, sizeof(uint32_t));
        in.read((char*)&iCount, sizeof(uint32_t));

        std::vector<SkinnedVertex> vertices(vCount);
        std::vector<uint32_t> indices(iCount);

        if (version == 1)
        {
            struct LegacySkinnedVertex
            {
                glm::vec3 Position;
                glm::vec3 Normal;
                glm::vec2 TexCoord;
                int32_t BoneIDs[4];
                float BoneWeights[4];
            };

            std::vector<LegacySkinnedVertex> legacyVertices(vCount);
            in.read((char*)legacyVertices.data(), vCount * sizeof(LegacySkinnedVertex));

            for (uint32_t i = 0; i < vCount; ++i)
            {
                vertices[i].Position = legacyVertices[i].Position;
                vertices[i].Normal = legacyVertices[i].Normal;
                vertices[i].TexCoord = legacyVertices[i].TexCoord;
                
                for (int j = 0; j < 4; ++j)
                {
                    vertices[i].BoneIDs[j] = legacyVertices[i].BoneIDs[j] >= 0 ? static_cast<uint8_t>(legacyVertices[i].BoneIDs[j]) : 0;
                    
                    float w = legacyVertices[i].BoneWeights[j];
                    vertices[i].BoneWeights[j] = static_cast<uint8_t>(glm::clamp(w * 255.0f, 0.0f, 255.0f));
                }
            }
        }
        else
        {
            in.read((char*)vertices.data(), vCount * sizeof(SkinnedVertex));
        }

        in.read((char*)indices.data(), iCount * sizeof(uint32_t));

        uint32_t submeshCount;
        in.read((char*)&submeshCount, sizeof(uint32_t));
        std::vector<Submesh> submeshes(submeshCount);

        for (uint32_t i = 0; i < submeshCount; i++)
        {
            in.read((char*)&submeshes[i].BaseVertex, sizeof(uint32_t));
            in.read((char*)&submeshes[i].BaseIndex, sizeof(uint32_t));
            in.read((char*)&submeshes[i].MaterialIndex, sizeof(uint32_t));
            in.read((char*)&submeshes[i].IndexCount, sizeof(uint32_t));
            in.read((char*)&submeshes[i].VertexCount, sizeof(uint32_t));
            in.read((char*)&submeshes[i].BoundingBox, sizeof(AABB));
            in.read((char*)&submeshes[i].LocalTransform, sizeof(glm::mat4));
            submeshes[i].NodeName = ReadString(in);

            if (version >= 3)
            {
                uint32_t lodCount = 0;
                in.read((char*)&lodCount, sizeof(uint32_t));
                submeshes[i].LODs.resize(lodCount);
                for (uint32_t l = 0; l < lodCount; ++l)
                {
                    in.read((char*)&submeshes[i].LODs[l].BaseIndex, sizeof(uint32_t));
                    in.read((char*)&submeshes[i].LODs[l].IndexCount, sizeof(uint32_t));
                }
            }
            else
            {
                submeshes[i].LODs.push_back({ submeshes[i].BaseIndex, submeshes[i].IndexCount });
            }

            uint32_t hullCount;
            in.read((char*)&hullCount, sizeof(uint32_t));
            submeshes[i].ConvexHulls.resize(hullCount);

            for (uint32_t h = 0; h < hullCount; h++)
            {
                uint32_t hvCount, hiCount;
                in.read((char*)&hvCount, sizeof(uint32_t));
                in.read((char*)&hiCount, sizeof(uint32_t));

                submeshes[i].ConvexHulls[h].Vertices.resize(hvCount);
                submeshes[i].ConvexHulls[h].Indices.resize(hiCount);

                in.read((char*)submeshes[i].ConvexHulls[h].Vertices.data(), hvCount * sizeof(glm::vec3));
                in.read((char*)submeshes[i].ConvexHulls[h].Indices.data(), hiCount * sizeof(uint32_t));
            }
        }

        uint32_t matCount;
        in.read((char*)&matCount, sizeof(uint32_t));
        std::vector<Ref<Material>> materials(matCount);

        auto assetManager = Application::Get().GetSubsystem<AssetManager>();

        for (uint32_t i = 0; i < matCount; i++)
        {
            std::string matPath = ReadString(in);

            if (!matPath.empty())
            {
                materials[i] = assetManager->GetMaterial(matPath);
            }
            else
            {
                Ref<Shader> defaultPBR = assetManager->GetShader("res/shaders/pbr.glsl");
                materials[i] = Material::CreateDefault(defaultPBR);
            }
        }

        Ref<Skeleton> skeleton = nullptr;
        bool hasSkeleton = false;
        in.read((char*)&hasSkeleton, sizeof(bool));
        if (hasSkeleton)
        {
            skeleton = CreateRef<Skeleton>();
            uint32_t jointCount = 0;
            in.read((char*)&jointCount, sizeof(uint32_t));

            for (uint32_t j = 0; j < jointCount; ++j)
            {
                std::string jointName = ReadString(in);
                int32_t parentIndex = -1;
                in.read((char*)&parentIndex, sizeof(int32_t));
                glm::mat4 invBindPose(1.0f);
                in.read((char*)&invBindPose, sizeof(glm::mat4));

                skeleton->AddJoint(jointName, parentIndex, invBindPose);
            }
            skeleton->Finalize();
        }

        return CreateRef<SkeletalMesh>(vertices, indices, submeshes, materials, skeleton);
    }

    Ref<SkeletalMesh> ModelSerializer::DeserializeSkeletal(const std::string& filepath)
    {
        auto vfs = Application::Get().GetSubsystem<VFSSystem>();
        if (vfs && vfs->FileExists(filepath))
        {
            auto fileData = vfs->ReadFile(filepath);
            std::string content((const char*)fileData.data(), fileData.size());
            std::istringstream in(content, std::ios::binary);
            return DeserializeSkeletalStream(in);
        }
        else
        {
            std::ifstream in(filepath, std::ios::binary);
            if (!in.is_open())
                return nullptr;
            return DeserializeSkeletalStream(in);
        }
    }
}