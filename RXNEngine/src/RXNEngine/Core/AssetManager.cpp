#include "rxnpch.h"
#include "AssetManager.h"

#include "RXNEngine/Renderer/ModelImporter.h"

namespace RXNEngine {

    std::unordered_map<std::string, Ref<StaticMesh>> AssetManager::s_Meshes;
    std::unordered_map<std::string, Ref<Shader>> AssetManager::s_Shaders;
    std::unordered_map<std::string, Ref<Texture2D>> AssetManager::m_Textures;


    Ref<Shader> AssetManager::GetShader(const std::string& path)
    {
        if (s_Shaders.find(path) != s_Shaders.end())
            return s_Shaders[path];

        Ref<Shader> shader = Shader::Create(path);
        s_Shaders[path] = shader;
        return shader;
    }

    Ref<StaticMesh> AssetManager::GetMesh(const std::string& path)
    {
        if (s_Meshes.find(path) != s_Meshes.end())
            return s_Meshes[path];

        Ref<StaticMesh> newMesh = ModelImporter::ImportAsset(path);

        if (newMesh)
            s_Meshes[path] = newMesh;

        return newMesh;
    }

    Ref<Texture2D> AssetManager::GetTexture(const std::string& path)
    {
        if (m_Textures.find(path) != m_Textures.end())
            return m_Textures[path];

        Ref<Texture2D> newTexture = Texture2D::Create(path);

        if (newTexture)
            m_Textures[path] = newTexture;

        return newTexture;
    }

    void AssetManager::Clear()
    {
        s_Meshes.clear();
        s_Shaders.clear();
    }
}