#include "rxnpch.h"
#include "AssetManager.h"

#include "RXNEngine/Core/JobSystem.h"
#include "RXNEngine/Scripting/ScriptEngine.h"
#include "RXNEngine/Core/Application.h"
#include "RXNEngine/Serialization/MaterialSerializer.h"
#include "RXNEngine/Serialization/PhysicsMaterialSerializer.h"

namespace RXNEngine {

    static void AttachMeshHierarchy(Ref<StaticMesh> mesh, const std::string& filepath, uint64_t entityID)
    {
        Scene* scene = Application::Get().GetSubsystem<ScriptEngine>()->GetSceneContext();
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
        if (m_Shaders.find(path) != m_Shaders.end())
            return m_Shaders[path];

        Ref<Shader> shader = Shader::Create(path);
        m_Shaders[path] = shader;
        return shader;
    }

    Ref<StaticMesh> AssetManager::GetMesh(const std::string& path)
    {
        if (m_Meshes.find(path) != m_Meshes.end())
            return m_Meshes[path];

        Ref<StaticMesh> newMesh = ModelImporter::ImportAsset(path);

        if (newMesh)
            m_Meshes[path] = newMesh;

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

    Ref<Material> AssetManager::GetMaterial(const std::string& path)
    {
        std::string normalizedPath = std::filesystem::path(path).generic_string();

        if (m_Materials.find(normalizedPath) != m_Materials.end())
            return m_Materials[normalizedPath];

        Ref<Shader> defaultShader = GetShader("res/shaders/pbr.glsl");
        Ref<Material> newMaterial = Material::CreateDefault(defaultShader);

        newMaterial->SetAssetPath(normalizedPath);

        MaterialSerializer serializer(newMaterial);
        if (serializer.Deserialize(normalizedPath))
        {
            m_Materials[normalizedPath] = newMaterial;
            return newMaterial;
        }

        RXN_CORE_ERROR("Could not load Material: {0}", normalizedPath);
        return nullptr;
    }

    Ref<PhysicsMaterial> AssetManager::GetPhysicsMaterial(const std::string& path)
    {
        std::string normalizedPath = std::filesystem::path(path).generic_string();

        if (m_PhysicsMaterials.find(normalizedPath) != m_PhysicsMaterials.end())
            return m_PhysicsMaterials[normalizedPath];

        Ref<PhysicsMaterial> newMaterial = CreateRef<PhysicsMaterial>();
        newMaterial->SetAssetPath(normalizedPath);

        PhysicsMaterialSerializer serializer(newMaterial);
        if (serializer.Deserialize(normalizedPath))
        {
            m_PhysicsMaterials[normalizedPath] = newMaterial;
            return newMaterial;
        }

        RXN_CORE_ERROR("Could not load PhysicsMaterial: {0}", normalizedPath);
        return nullptr;
    }

    void AssetManager::Clear()
    {
        m_Meshes.clear();
        m_Shaders.clear();
        m_Textures.clear();
        m_Materials.clear();
		m_PhysicsMaterials.clear();
    }

    void AssetManager::LoadMeshAsync(const std::string& path, uint64_t entityID)
    {
        if (m_Meshes.find(path) != m_Meshes.end())
        {
            AttachMeshHierarchy(m_Meshes[path], path, entityID);
            return;
        }

        Application::Get().GetSubsystem<JobSystem>()->Execute([this, path, entityID]()
            {
                AsyncLoadTask* task = new AsyncLoadTask();
                task->EntityID = entityID;
                task->Filepath = path;

                if (ModelImporter::LoadModelData(path, task->Data))
                {
                    std::lock_guard<std::mutex> lock(m_AsyncMutex);
                    m_FinishedTasks.push_back(task);
                }
                else
                {
                    delete task;
                }
            });
    }

    void AssetManager::Update(float deltaTime)
    {
        std::lock_guard<std::mutex> lock(m_AsyncMutex);
        if (m_FinishedTasks.empty()) return;

        OPTICK_EVENT("AssetManager::UploadToGPU");

        AsyncLoadTask* task = m_FinishedTasks.front();
        m_FinishedTasks.erase(m_FinishedTasks.begin());

        Ref<StaticMesh> gpuMesh = ModelImporter::BuildMeshFromData(task->Data, task->Filepath);

        m_Meshes[task->Filepath] = gpuMesh;

        AttachMeshHierarchy(gpuMesh, task->Filepath, task->EntityID);

        RXN_CORE_INFO("Async Load Complete! Mesh attached to Entity {0}", task->EntityID);

        delete task;
    }
}