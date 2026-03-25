#include "rxnpch.h"
#include "AssetManager.h"

#include "RXNEngine/Core/JobSystem.h"
#include "RXNEngine/Scripting/ScriptEngine.h"

namespace RXNEngine {

    std::mutex AssetManager::s_AsyncMutex;
    std::vector<AssetManager::AsyncLoadTask*> AssetManager::s_FinishedTasks;

    std::unordered_map<std::string, Ref<StaticMesh>> AssetManager::s_Meshes;
    std::unordered_map<std::string, Ref<Shader>> AssetManager::s_Shaders;
    std::unordered_map<std::string, Ref<Texture2D>> AssetManager::m_Textures;


    static void AttachMeshHierarchy(Ref<StaticMesh> mesh, const std::string& filepath, uint64_t entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        if (!scene) return;

        Entity rootEntity = scene->GetEntityByUUID(entityID);
        if (!rootEntity) return;

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
    }


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

    void AssetManager::LoadMeshAsync(const std::string& path, uint64_t entityID)
    {
        if (s_Meshes.find(path) != s_Meshes.end())
        {
            AttachMeshHierarchy(s_Meshes[path], path, entityID);
            return;
        }

        JobSystem::Execute([path, entityID]()
            {
                AsyncLoadTask* task = new AsyncLoadTask();
                task->EntityID = entityID;
                task->Filepath = path;

                if (ModelImporter::LoadModelData(path, task->Data))
                {
                    std::lock_guard<std::mutex> lock(s_AsyncMutex);
                    s_FinishedTasks.push_back(task);
                }
                else
                {
                    delete task;
                }
            });
    }

    void AssetManager::Update()
    {
        std::lock_guard<std::mutex> lock(s_AsyncMutex);
        if (s_FinishedTasks.empty()) return;

        OPTICK_EVENT("AssetManager::UploadToGPU");

        AsyncLoadTask* task = s_FinishedTasks.front();
        s_FinishedTasks.erase(s_FinishedTasks.begin());

        Ref<StaticMesh> gpuMesh = ModelImporter::BuildMeshFromData(task->Data);

        s_Meshes[task->Filepath] = gpuMesh;

        AttachMeshHierarchy(gpuMesh, task->Filepath, task->EntityID);

        RXN_CORE_INFO("Async Load Complete! Mesh attached to Entity {0}", task->EntityID);

        delete task;
    }
}