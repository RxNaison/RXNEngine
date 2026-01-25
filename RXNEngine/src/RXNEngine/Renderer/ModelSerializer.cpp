#include "rxnpch.h"
#include "ModelSerializer.h"

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

    void ModelSerializer::Serialize(const std::string& filepath, const Model& model)
    {
        std::ofstream out(filepath, std::ios::binary);
        if (!out.is_open())
        {
            RXN_CORE_ERROR("Failed to write binary asset: {0}", filepath);
            return;
        }

        const char* magic = "RXN\0";
        out.write(magic, 4);

        uint32_t version = 1;
        out.write((char*)&version, sizeof(uint32_t));

        const auto& submeshes = model.GetSubmeshes();
        uint32_t submeshCount = (uint32_t)submeshes.size();
        out.write((char*)&submeshCount, sizeof(uint32_t));

        for (const auto& submesh : submeshes)
        {
            out.write((char*)&submesh.BoundingBox, sizeof(AABB));
            out.write((char*)&submesh.LocalTransform, sizeof(glm::mat4));

            const auto& vertices = submesh.Geometry->GetVertices();
            const auto& indices = submesh.Geometry->GetIndices();

            uint32_t vCount = (uint32_t)vertices.size();
            uint32_t iCount = (uint32_t)indices.size();
            out.write((char*)&vCount, sizeof(uint32_t));
            out.write((char*)&iCount, sizeof(uint32_t));

            out.write((char*)vertices.data(), vCount * sizeof(float));
            out.write((char*)indices.data(), iCount * sizeof(uint32_t));

            auto mat = submesh.Surface;

            auto params = mat->GetParameters();
            out.write((char*)&params, sizeof(Material::Parameters));

            std::string albedoPath = mat->GetAlbedoMap() ? mat->GetAlbedoMap()->GetPath() : "";
            WriteString(out, albedoPath);

            std::string normalPath = mat->GetNormalMap() ? mat->GetNormalMap()->GetPath() : "";
            WriteString(out, normalPath);

            std::string metalPath = mat->GetMetalnessRoughnessMap() ? mat->GetMetalnessRoughnessMap()->GetPath() : "";
            WriteString(out, metalPath);

            std::string aoPath = mat->GetAOMap() ? mat->GetAOMap()->GetPath() : "";
            WriteString(out, aoPath);

			bool isTransparent = mat->IsTransparent();
            out.write((char*)&isTransparent, sizeof(bool));
        }

        out.close();
    }

    bool ModelSerializer::Deserialize(const std::string& filepath, Model& outModel)
    {
        std::ifstream in(filepath, std::ios::binary);
        if (!in.is_open()) return false;

        char magic[4];
        in.read(magic, 4);
        if (std::string(magic) != "RXN\0") return false;

        uint32_t version;
        in.read((char*)&version, sizeof(uint32_t));
        if (version != 1) return false;

        uint32_t submeshCount;
        in.read((char*)&submeshCount, sizeof(uint32_t));

        auto LoadCachedTexture = [&](const std::string& path) -> Ref<Texture2D>
            {
                if (path.empty()) return nullptr;

                if (outModel.m_TextureCache.find(path) != outModel.m_TextureCache.end())
                    return outModel.m_TextureCache[path];

                Ref<Texture2D> texture = Texture2D::Create(path);
                outModel.m_TextureCache[path] = texture; 

                return texture;
            };

        for (uint32_t i = 0; i < submeshCount; i++)
        {
            ModelSubmesh submesh;

            in.read((char*)&submesh.BoundingBox, sizeof(AABB));
            in.read((char*)&submesh.LocalTransform, sizeof(glm::mat4));

            uint32_t vertexCount = 0;
            uint32_t indexCount = 0;
            in.read((char*)&vertexCount, sizeof(uint32_t));
            in.read((char*)&indexCount, sizeof(uint32_t));

            std::vector<float> vertices(vertexCount);
            std::vector<uint32_t> indices(indexCount);

            in.read((char*)vertices.data(), vertexCount * sizeof(float));
            in.read((char*)indices.data(), indexCount * sizeof(uint32_t));

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

            submesh.Geometry = CreateRef<Mesh>(vao);
            submesh.Geometry->SetAABB(submesh.BoundingBox);

            const_cast<std::vector<float>&>(submesh.Geometry->GetVertices()) = vertices;
            const_cast<std::vector<uint32_t>&>(submesh.Geometry->GetIndices()) = indices;

            Ref<Material> mat = Material::CreateDefault(outModel.m_DefaultShader);

            Material::Parameters params;
            in.read((char*)&params, sizeof(Material::Parameters));

            mat->SetAlbedoColor(params.AlbedoColor);
            mat->SetMetalness(params.Metalness);
            mat->SetRoughness(params.Roughness);
            mat->SetEmissiveColor(params.EmissiveColor);
            mat->SetAO(params.AO);

            std::string path;

            path = ReadString(in);
            if (!path.empty())
                mat->SetAlbedoMap(LoadCachedTexture(path));

            path = ReadString(in);
            if (!path.empty())
                mat->SetNormalMap(LoadCachedTexture(path));

            path = ReadString(in);
            if (!path.empty())
                mat->SetMetalnessRoughnessMap(LoadCachedTexture(path));

            path = ReadString(in);
            if (!path.empty())
                mat->SetAOMap(LoadCachedTexture(path));

            bool isTransparent = false;
            in.read((char*)&isTransparent, sizeof(bool));
			mat->SetTransparent(isTransparent);

            submesh.Surface = mat;

            outModel.m_Submeshes.push_back(submesh);
        }

        return true;
    }
}