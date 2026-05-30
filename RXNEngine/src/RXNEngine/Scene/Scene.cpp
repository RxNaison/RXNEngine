#include "rxnpch.h"
#include "Scene.h"
#include "Components.h"
#include "Entity.h"
#include "RXNEngine/Renderer/Renderer.h"
#include "RXNEngine/Renderer/Renderer2D.h"
#include "RXNEngine/Physics/PhysicsSystem.h"
#include "RXNEngine/Physics/PhysicsWorld.h"
#include "RXNEngine/Scripting/ScriptEngine.h"
#include "RXNEngine/Core/JobSystem.h"
#include "RXNEngine/Asset/AssetManager.h"
#include "RXNEngine/Audio/AudioSystem.h"
#include "RXNEngine/Core/Input.h"
#include "RXNEngine/Renderer/RenderCommand.h"
#include "RXNEngine/UI/UISystem.h"

namespace RXNEngine {

    struct ViewFrustum
    {
        glm::vec4 Planes[6];

        void Extract(const glm::mat4& viewProj)
        {
            for (int i = 0; i < 4; ++i)
                Planes[0][i] = viewProj[i][3] + viewProj[i][0]; // Left

            for (int i = 0; i < 4; ++i)
                Planes[1][i] = viewProj[i][3] - viewProj[i][0]; // Right

            for (int i = 0; i < 4; ++i)
                Planes[2][i] = viewProj[i][3] + viewProj[i][1]; // Bottom

            for (int i = 0; i < 4; ++i)
                Planes[3][i] = viewProj[i][3] - viewProj[i][1]; // Top

            for (int i = 0; i < 4; ++i)
                Planes[4][i] = viewProj[i][3] + viewProj[i][2]; // Near

            for (int i = 0; i < 4; ++i)
                Planes[5][i] = viewProj[i][3] - viewProj[i][2]; // Far

            for (int i = 0; i < 6; ++i)
            {
                float length = glm::length(glm::vec3(Planes[i]));
                Planes[i] /= length;
            }
        }

        bool IsSphereVisible(const glm::vec3& center, float radius) const
        {
            for (int i = 0; i < 6; ++i)
            {
                if (glm::dot(glm::vec3(Planes[i]), center) + Planes[i].w < -radius)
                    return false;
            }
            return true;
        }
    };

    struct RenderCommandData
    {
        Ref<StaticMesh> Mesh;
        uint32_t SubmeshIndex;
        Ref<Material> Material;
        glm::mat4 Transform;
        int EntityID;
        bool IsVisibleToCamera;
        bool IsVisibleToShadows;
        glm::vec3 BoundingCenter;
        float BoundingRadius;
    };

    template<typename T>
    static void CopyComponent(entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& enttMap)
    {
        auto view = src.view<T>();
        for (auto e : view)
        {
            UUID uuid = src.get<IDComponent>(e).ID;
            if (enttMap.find(uuid) == enttMap.end())
                continue;

            entt::entity dstEntID = enttMap.at(uuid);
            auto& component = src.get<T>(e);
            dst.emplace_or_replace<T>(dstEntID, component);
        }
    }

    template<typename... Component>
    static void CopyAllComponents(entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& enttMap, ComponentGroup<Component...>)
    {
        (CopyComponent<Component>(dst, src, enttMap), ...);
    }

    template<typename... Component>
    static void CopyComponents(ComponentGroup<Component...>, Entity src, Entity dst)
    {
        ([&]()
            {
                if (src.HasComponent<Component>())
                {
                    if (dst.HasComponent<Component>())
                        dst.GetComponent<Component>() = src.GetComponent<Component>();
                    else
                        dst.AddComponent<Component>(src.GetComponent<Component>());
                }
            }(), ...);
    }

    Ref<Scene> Scene::Copy(Ref<Scene> other)
    {
        RXN_PROFILE_SCOPE();
        Ref<Scene> newScene = CreateRef<Scene>();

        newScene->OnViewportResize(other->m_ViewportWidth, other->m_ViewportHeight);
        newScene->m_Skybox = other->m_Skybox;
        newScene->m_SkyboxIntensity = other->m_SkyboxIntensity;
        newScene->m_PrimaryCameraID = other->m_PrimaryCameraID;
        newScene->m_RendererSettings = other->m_RendererSettings;

        auto& srcSceneRegistry = other->m_Registry;
        auto& dstSceneRegistry = newScene->m_Registry;
        std::unordered_map<UUID, entt::entity> enttMap;

        auto idView = srcSceneRegistry.view<IDComponent>();
        for (auto e : idView)
        {
            UUID uuid = srcSceneRegistry.get<IDComponent>(e).ID;
            const auto& name = srcSceneRegistry.get<TagComponent>(e).Tag;
            Entity newEntity = newScene->CreateEntityWithUUID(uuid, name);
            enttMap[uuid] = (entt::entity)newEntity;
        }

        CopyAllComponents(dstSceneRegistry, srcSceneRegistry, enttMap, AllComponents{});
        return newScene;
    }

    Scene::Scene()
    {
        m_Registry.on_construct<CameraComponent>().connect<&Scene::OnCameraComponentAdded>(this);
        AddSubsystem<PhysicsWorld>();
        AddSubsystem<UISystem>(this);
    }

    Scene::~Scene()
    {
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        return CreateEntityWithUUID(UUID(), name);
    }

    Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
    {
        if (m_EntityMap.find(uuid) != m_EntityMap.end())
        {
            entt::entity oldEnt = m_EntityMap[uuid];
            if (m_Registry.valid(oldEnt))
                m_Registry.destroy(oldEnt);
        }

        Entity entity = { m_Registry.create(), this };
        entity.AddComponent<IDComponent>(uuid);
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<RelationshipComponent>();
        auto& tag = entity.AddComponent<TagComponent>();
        tag.Tag = name.empty() ? "Entity" : name;

        m_EntityMap[uuid] = (entt::entity)entity;
        return entity;
    }

    void Scene::DestroyEntity(Entity entity)
    {
        m_EntitiesToDestroy.insert((entt::entity)entity);
    }

    void Scene::RemoveEntity(Entity entity)
    {
        RXN_PROFILE_SCOPE();

        if (m_IsRunning && entity.HasComponent<ScriptComponent>())
            Application::Get().GetSubsystem<ScriptEngine>()->OnDestroyEntity(entity);

        auto physicsWorld = GetSubsystem<PhysicsWorld>();

        if (physicsWorld)
            physicsWorld->DestroyPhysicsBody(entity);

        if (entity.HasComponent<AudioSourceComponent>())
        {
            auto& ac = entity.GetComponent<AudioSourceComponent>();
            if (ac.RuntimeSound)
            {
                auto audioBackend = Application::Get().GetSubsystem<AudioSystem>()->GetBackend();
                audioBackend->DestroySoundSource(ac.RuntimeSound);
                ac.RuntimeSound = nullptr;
            }
        }

        auto& rc = entity.GetComponent<RelationshipComponent>();
        std::vector<UUID> childrenToDestroy = rc.Children;
        for (auto childID : childrenToDestroy)
        {
            Entity child = GetEntityByUUID(childID);
            if (child)
                RemoveEntity(child);
        }

        if (rc.ParentHandle != 0)
        {
            Entity parent = GetEntityByUUID(rc.ParentHandle);
            if (parent)
            {
                auto& parentRC = parent.GetComponent<RelationshipComponent>();
                auto it = std::find(parentRC.Children.begin(), parentRC.Children.end(), entity.GetUUID());
                if (it != parentRC.Children.end())
                    parentRC.Children.erase(it);
            }
        }

        m_EntityMap.erase(entity.GetUUID());
        m_Registry.destroy(entity);
    }

    void Scene::ProcessDeletedEntities()
    {
        if (m_EntitiesToDestroy.empty())
            return;

        for (auto entityID : m_EntitiesToDestroy)
        {
            if (m_Registry.valid(entityID))
                RemoveEntity({ entityID, this });
        }
        m_EntitiesToDestroy.clear();
    }

    void Scene::ParentEntity(Entity entity, Entity parent)
    {
        if (entity == parent)
            return;

        if (parent && IsDescendantOf(parent, entity))
            return;

        auto& rc = entity.GetComponent<RelationshipComponent>();
        if (rc.ParentHandle != 0)
        {
            Entity oldParent = GetEntityByUUID(rc.ParentHandle);
            if (oldParent)
            {
                auto& oldParentRC = oldParent.GetComponent<RelationshipComponent>();
                auto it = std::find(oldParentRC.Children.begin(), oldParentRC.Children.end(), entity.GetUUID());
                if (it != oldParentRC.Children.end())
                    oldParentRC.Children.erase(it);
            }
        }

        rc.ParentHandle = parent ? parent.GetUUID() : UUID::Null;
        if (parent)
        {
            auto& parentRC = parent.GetComponent<RelationshipComponent>();
            parentRC.Children.push_back(entity.GetUUID());
        }
    }

    bool Scene::IsDescendantOf(Entity entity, Entity potentialAscendant)
    {
        auto& rc = entity.GetComponent<RelationshipComponent>();

        if (rc.ParentHandle == 0)
            return false;

        if (rc.ParentHandle == potentialAscendant.GetUUID())
            return true;

        Entity parent = GetEntityByUUID(rc.ParentHandle);

        if (!parent)
            return false;

        return IsDescendantOf(parent, potentialAscendant);
    }

    void Scene::UpdateWorldTransforms()
    {
        RXN_PROFILE_SCOPE();

        std::vector<std::vector<entt::entity>> generations;
        generations.push_back({});

        auto view = m_Registry.view<TransformComponent, RelationshipComponent>();
        for (auto e : view)
        {
            if (view.get<RelationshipComponent>(e).ParentHandle == 0)
                generations[0].push_back(e);
        }

        int currentGen = 0;
        while (!generations[currentGen].empty())
        {
            std::vector<entt::entity> nextGen;

            for (auto e : generations[currentGen])
            {
                auto& rc = m_Registry.get<RelationshipComponent>(e);
                for (UUID childID : rc.Children)
                {
                    Entity child = GetEntityByUUID(childID);
                    if (child)
                        nextGen.push_back((entt::entity)child);
                }
            }

            if (!nextGen.empty())
            {
                generations.push_back(nextGen);
                currentGen++;
            }
            else break;
        }

        auto jobSys = Application::Get().GetSubsystem<JobSystem>();
        uint32_t threadCount = jobSys->GetThreadCount();

        for (size_t i = 0; i < generations.size(); i++)
        {
            auto& gen = generations[i];
            uint32_t groupSize = (uint32_t)gen.size() / threadCount;
            if (groupSize == 0) groupSize = 1;

            jobSys->Dispatch(gen.size(), groupSize, [this, &gen, i](JobDispatchArgs args)
                {
                    entt::entity e = gen[args.JobIndex];
                    auto& tc = m_Registry.get<TransformComponent>(e);

                    if (i == 0)
                    {
                        tc.WorldTransform = tc.GetTransform();
                    }
                    else
                    {
                        auto& rc = m_Registry.get<RelationshipComponent>(e);
                        Entity parent = GetEntityByUUID(rc.ParentHandle);
                        auto& parentTc = parent.GetComponent<TransformComponent>();
                        tc.WorldTransform = parentTc.WorldTransform * tc.GetTransform();
                    }
                });

            jobSys->Wait();
        }
    }

    glm::mat4 Scene::GetWorldTransform(Entity entity)
    {
        return entity.GetComponent<TransformComponent>().WorldTransform;
    }

    void Scene::OnUpdateEditor(float deltaTime)
    {
        RXN_PROFILE_SCOPE();

        ProcessDeletedEntities();
        UpdateWorldTransforms();
        GetSubsystem<UISystem>()->Update(deltaTime);
    }

    void Scene::OnUpdateSimulation(float deltaTime)
    {
        RXN_PROFILE_SCOPE();
        auto physicsWorld = GetSubsystem<PhysicsWorld>();

        if (m_IsRunning)
        {
            auto scriptView = m_Registry.view<ScriptComponent>();
            std::vector<entt::entity> entities(scriptView.begin(), scriptView.end());

            if (!entities.empty())
            {
                RXN_PROFILE_SCOPE_NAMED("Run C# Fixed Update");
                for (auto e : entities)
                {
                    Entity entity = { e, this };
                    Application::Get().GetSubsystem<ScriptEngine>()->OnFixedUpdateEntity(entity, deltaTime);
                }
            }

            UpdateWorldTransforms();

            if (physicsWorld)
            {
                physicsWorld->LockWrite();

                auto transformView = m_Registry.view<TransformComponent>();
                for (auto e : transformView)
                {
                    auto& tc = transformView.get<TransformComponent>(e);
                    if (tc.IsDirty)
                    {
                        Entity entity = { e, this };

                        if (entity.HasComponent<RigidbodyComponent>())
                            physicsWorld->SyncTransformToPhysics(entity);

                        tc.IsDirty = false;
                    }
                }

                physicsWorld->UnlockWrite();
            }
        }

        if (physicsWorld)
            physicsWorld->UpdateCCDFlags(this);

        for (auto& subsystem : m_SubsystemList)
            subsystem->Update(deltaTime);

        if (physicsWorld)
            physicsWorld->SyncPhysicsToTransforms(this);

        ProcessDeletedEntities();
    }

    void Scene::OnUpdateRuntime(float deltaTime)
    {
        RXN_PROFILE_SCOPE();

        auto spotLightView = m_Registry.view<SpotLightComponent>();
        for (auto e : spotLightView)
        {
            auto& sl = spotLightView.get<SpotLightComponent>(e);
            if (sl.IsVideo && sl.CookieVideo)
                sl.CookieVideo->Update(deltaTime);
        }

        Entity cameraEntity = GetPrimaryCameraEntity();
        if (cameraEntity)
        {
            glm::mat4 camTransform = GetWorldTransform(cameraEntity);
            glm::vec3 camPos = glm::vec3(camTransform[3]);
            glm::vec3 camForward = -glm::normalize(glm::vec3(camTransform[2]));
            glm::vec3 camUp = glm::normalize(glm::vec3(camTransform[1]));

            Application::Get().GetSubsystem<AudioSystem>()->GetBackend()->UpdateListener(camPos, camForward, camUp);
        }

        auto audioBackend = Application::Get().GetSubsystem<AudioSystem>()->GetBackend();
        auto audioView = m_Registry.view<TransformComponent, AudioSourceComponent>();
        for (auto e : audioView)
        {
            auto [tc, ac] = audioView.get<TransformComponent, AudioSourceComponent>(e);
            if (ac.RuntimeSound)
            {
                glm::vec3 worldPos = glm::vec3(GetWorldTransform({ e, this })[3]);
                audioBackend->UpdateSoundSource(ac.RuntimeSound, worldPos, ac.Volume, 1.0f, ac.MinDistance, ac.MaxDistance);
            }
        }

        auto scriptSys = Application::Get().GetSubsystem<ScriptEngine>();
        scriptSys->SetEngineTime(deltaTime);

        m_Registry.view<NativeScriptComponent>().each([=](auto entity, auto& nsc)
            {
                if (!nsc.Instance)
                {
                    nsc.Instance = nsc.InstantiateScript();
                    nsc.Instance->m_EntityHandle = entity;
                    nsc.Instance->m_Scene = this;
                    nsc.Instance->OnCreate();
                }
                nsc.Instance->OnUpdate(deltaTime);
            });

        auto scriptView = m_Registry.view<ScriptComponent>();
        std::vector<entt::entity> entities(scriptView.begin(), scriptView.end());

        if (!entities.empty())
        {
            RXN_PROFILE_SCOPE_NAMED("Run C# Update");
            for (auto e : entities)
            {
                Entity entity = { e, this };
                scriptSys->OnUpdateEntity(entity, deltaTime);
            }
        }

        ProcessDeletedEntities();

        UpdateWorldTransforms();
        GetSubsystem<UISystem>()->Update(deltaTime);
    }

    void Scene::OnViewportResize(uint32_t width, uint32_t height)
    {
        m_ViewportWidth = width;
        m_ViewportHeight = height;

        auto view = m_Registry.view<CameraComponent>();
        for (auto entity : view)
        {
            auto& cameraComponent = view.get<CameraComponent>(entity);
            if (!cameraComponent.FixedAspectRatio)
                cameraComponent.Camera.SetViewportSize(width, height);
        }
    }

    Entity Scene::GetEntityByUUID(UUID uuid)
    {
        if (m_EntityMap.find(uuid) != m_EntityMap.end())
            return { m_EntityMap.at(uuid), this };

        return {};
    }

    Entity Scene::DuplicateEntity(Entity entity)
    {
        RXN_PROFILE_SCOPE();

        std::string name = entity.GetComponent<TagComponent>().Tag;
        Entity newEntity = CreateEntity(name + " (Clone)");

        CopyComponents(AllComponents{}, entity, newEntity);

        if (newEntity.HasComponent<RelationshipComponent>())
        {
            auto& clonedRC = newEntity.GetComponent<RelationshipComponent>();
            clonedRC.ParentHandle = 0;
            clonedRC.Children.clear();
        }

        if (newEntity.HasComponent<RigidbodyComponent>())
        {
            newEntity.GetComponent<RigidbodyComponent>().RuntimeActor = nullptr;

            if (newEntity.HasComponent<BoxColliderComponent>())
            {
                newEntity.GetComponent<BoxColliderComponent>().RuntimeShape = nullptr;
                newEntity.GetComponent<BoxColliderComponent>().RuntimeMaterial = nullptr;
            }
            if (newEntity.HasComponent<SphereColliderComponent>())
            {
                newEntity.GetComponent<SphereColliderComponent>().RuntimeShape = nullptr;
                newEntity.GetComponent<SphereColliderComponent>().RuntimeMaterial = nullptr;
            }
            if (newEntity.HasComponent<CapsuleColliderComponent>())
            {
                newEntity.GetComponent<CapsuleColliderComponent>().RuntimeShape = nullptr;
                newEntity.GetComponent<CapsuleColliderComponent>().RuntimeMaterial = nullptr;
            }

            if (m_IsSimulating)
            {
                auto physicsWorld = GetSubsystem<PhysicsWorld>();
                if (physicsWorld)
                    physicsWorld->CreatePhysicsBody(newEntity);
            }
        }

        if (newEntity.HasComponent<CharacterControllerComponent>())
            newEntity.GetComponent<CharacterControllerComponent>().RuntimeController = nullptr;

        if (newEntity.HasComponent<ScriptComponent>())
        {
            if (m_IsRunning)
                Application::Get().GetSubsystem<ScriptEngine>()->OnCreateEntity(newEntity);
        }

        if (entity.HasComponent<RelationshipComponent>())
        {
            auto& originalRC = entity.GetComponent<RelationshipComponent>();
            for (UUID childID : originalRC.Children)
            {
                Entity child = GetEntityByUUID(childID);
                if (child)
                {
                    Entity clonedChild = DuplicateEntity(child);
                    clonedChild.GetComponent<RelationshipComponent>().ParentHandle = newEntity.GetUUID();
                    newEntity.GetComponent<RelationshipComponent>().Children.push_back(clonedChild.GetUUID());
                }
            }
        }

        return newEntity;
    }

    Entity Scene::FindEntityByName(std::string_view name)
    {
        auto view = m_Registry.view<TagComponent>();
        for (auto entityID : view)
        {
            const auto& tc = view.get<TagComponent>(entityID);
            if (tc.Tag == name)
                return Entity{ entityID, this };
        }
        return {};
    }

    Entity Scene::GetPrimaryCameraEntity()
    {
        Entity entity = GetEntityByUUID(m_PrimaryCameraID);
        if (entity && entity.HasComponent<CameraComponent>())
            return entity;

        return {};
    }

    void Scene::SetPrimaryCameraEntity(Entity entity)
    {
        if (entity.HasComponent<CameraComponent>())
            m_PrimaryCameraID = entity.GetUUID();
    }

    void Scene::OnCameraComponentAdded(entt::registry& registry, entt::entity entity)
    {
        if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
        {
            auto& cameraComponent = registry.get<CameraComponent>(entity);
            if (!cameraComponent.FixedAspectRatio)
                cameraComponent.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
        }
    }

    void Scene::OnRuntimeStart()
    {
        OnSimulationStart();

        auto scriptSys = Application::Get().GetSubsystem<ScriptEngine>();
        scriptSys->OnRuntimeStart(this);

        auto view = m_Registry.view<ScriptComponent>();
        for (auto e : view)
        {
            Entity entity = { e, this };
            scriptSys->OnCreateEntity(entity);
        }

        auto spotLightView = m_Registry.view<SpotLightComponent>();
        for (auto e : spotLightView)
        {
            auto& sl = spotLightView.get<SpotLightComponent>(e);
            if (sl.IsVideo && sl.CookieVideo)
                sl.CookieVideo->Rewind();
        }

        auto audioBackend = Application::Get().GetSubsystem<AudioSystem>()->GetBackend();
        auto audioView = m_Registry.view<AudioSourceComponent>();
        for (auto e : audioView)
        {
            auto& ac = audioView.get<AudioSourceComponent>(e);
            if (!ac.AudioClipPath.empty())
            {
                ac.RuntimeSound = audioBackend->CreateSoundSource(ac.AudioClipPath, ac.Looping, ac.MinDistance, ac.MaxDistance);
                if (ac.RuntimeSound && ac.PlayOnAwake)
                {
                    audioBackend->PlaySoundSource(ac.RuntimeSound);
                    ac.IsPlaying = true;
                }
            }
        }

        m_IsRunning = true;
    }

    void Scene::OnRuntimeStop()
    {
        m_IsRunning = false;
        OnSimulationStop();
        Application::Get().GetSubsystem<ScriptEngine>()->OnRuntimeStop();

        auto spotLightView = m_Registry.view<SpotLightComponent>();
        for (auto e : spotLightView)
        {
            auto& sl = spotLightView.get<SpotLightComponent>(e);
            if (sl.IsVideo && sl.CookieVideo)
                sl.CookieVideo->Rewind();
        }

        auto audioBackend = Application::Get().GetSubsystem<AudioSystem>()->GetBackend();
        auto audioView = m_Registry.view<AudioSourceComponent>();
        for (auto e : audioView)
        {
            auto& ac = audioView.get<AudioSourceComponent>(e);
            if (ac.RuntimeSound)
            {
                audioBackend->DestroySoundSource(ac.RuntimeSound);
                ac.RuntimeSound = nullptr;
                ac.IsPlaying = false;
            }
        }
    }

    void Scene::OnSimulationStart()
    {
        UpdateWorldTransforms();

        auto physicsWorld = GetSubsystem<PhysicsWorld>();
        if (physicsWorld)
            physicsWorld->OnSimulationStart(this);

        m_IsSimulating = true;
    }

    void Scene::OnSimulationStop()
    {
        auto physicsWorld = GetSubsystem<PhysicsWorld>();
        if (physicsWorld)
            physicsWorld->OnSimulationStop(this);

        m_IsSimulating = false;
    }
}