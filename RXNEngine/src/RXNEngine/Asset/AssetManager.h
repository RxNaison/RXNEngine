#pragma once
#include "RXNEngine/Core/Subsystem.h"
#include "RXNEngine/Asset/StaticMesh.h"
#include "RXNEngine/Renderer/GraphicsAPI/Shader.h"
#include "RXNEngine/Asset/ModelImporter.h"

#include <unordered_map>
#include <string>

namespace RXNEngine {

    class AssetManager : public Subsystem
    {
    public:
        virtual void Init() override { RXN_CORE_INFO("AssetManager Initialized"); }
        virtual void Update(float deltaTime) override;
        virtual void Shutdown() override { Clear(); }

        Ref<StaticMesh> GetMesh(const std::string& path);
        Ref<Texture2D> GetTexture(const std::string& path);
        Ref<Shader> GetShader(const std::string& path);
        Ref<Material> GetMaterial(const std::string& path);

        void Clear();

        void LoadMeshAsync(const std::string& path, uint64_t entityID);

    private:
        struct AsyncLoadTask {
            uint64_t EntityID;
            std::string Filepath;
            ImporterData Data;
        };

        std::mutex m_AsyncMutex;
        std::vector<AsyncLoadTask*> m_FinishedTasks;

        std::unordered_map<std::string, Ref<StaticMesh>> m_Meshes;
        std::unordered_map<std::string, Ref<Shader>> m_Shaders;
        std::unordered_map<std::string, Ref<Texture2D>> m_Textures;
        std::unordered_map<std::string, Ref<Material>> m_Materials;
    };

}