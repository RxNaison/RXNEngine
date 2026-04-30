#include "rxnpch.h"
#include "ModelSerializer.h"
#include "RXNEngine/Asset/AssetManager.h"
#include <fstream>

namespace RXNEngine {

    static void WriteString(std::ofstream& out, const std::string& str)
    {
        uint32_t size = (uint32_t)str.size();
        out.write((char*)&size, sizeof(uint32_t));
        if (size > 0)
            out.write(str.c_str(), size);
    }

    static std::string ReadString(std::ifstream& in)
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

        uint32_t version = 4;
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
            WriteString(out, mat->GetAssetPath());

        out.close();
    }

    Ref<StaticMesh> ModelSerializer::Deserialize(const std::string& filepath)
    {
        std::ifstream in(filepath, std::ios::binary);
        if (!in.is_open()) return nullptr;

        char magic[4];
        in.read(magic, 4);
        if (std::string(magic) != "RXN\0") return nullptr;

        uint32_t version;
        in.read((char*)&version, sizeof(uint32_t));
        if (version != 4)
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
}