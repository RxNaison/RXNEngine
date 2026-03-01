#pragma once
#include "RXNEngine/Renderer/StaticMesh.h"
#include "RXNEngine/Renderer/Shader.h"
#include <unordered_map>
#include <string>

namespace RXNEngine {

    class AssetManager
    {
    public:
        static Ref<StaticMesh> GetMesh(const std::string& path);
        static Ref<Texture2D> GetTexture(const std::string& path);
        static Ref<Shader> GetShader(const std::string& path);

        static void Clear();

    private:
        static std::unordered_map<std::string, Ref<StaticMesh>> s_Meshes;
        static std::unordered_map<std::string, Ref<Shader>> s_Shaders;
        static std::unordered_map<std::string, Ref<Texture2D>> m_Textures;
    };

}