#include "rxnpch.h"
#include "Scene.h"
#include "Components.h"
#include "Entity.h"
#include "RXNEngine/Renderer/Renderer.h"
#include "RXNEngine/Physics/PhysicsSystem.h"
#include "RXNEngine/Physics/PhysicsWorld.h"
#include "RXNEngine/Scripting/ScriptEngine.h"
#include "RXNEngine/Core/JobSystem.h"
#include "RXNEngine/Asset/AssetManager.h"

namespace RXNEngine {

    namespace PhysicsUtils {
        inline physx::PxVec3 GLMToPhysX(const glm::vec3& vec) { return { vec.x, vec.y, vec.z }; }
        inline glm::vec3 PhysXToGLM(const physx::PxVec3& vec) { return { vec.x, vec.y, vec.z }; }

        inline physx::PxQuat GLMToPhysX(const glm::quat& q) { return { q.x, q.y, q.z, q.w }; }
        inline glm::quat PhysXToGLM(const physx::PxQuat& q) { return { q.w, q.x, q.y, q.z }; }

        static glm::vec4 UnpackPhysXColor(physx::PxU32 color)
        {
            float b = (float)((color >> 16) & 0xFF) / 255.0f;
            float g = (float)((color >> 8) & 0xFF) / 255.0f;
            float r = (float)((color >> 0) & 0xFF) / 255.0f;
            return { r, g, b, 1.0f };
        }
    }

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

            for (int i = 0; i < 6; ++i) {
                float length = glm::length(glm::vec3(Planes[i]));
                Planes[i] /= length;
            }
        }

        bool IsSphereVisible(const glm::vec3& center, float radius) const
        {
            for (int i = 0; i < 6; ++i) {
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
        OPTICK_EVENT();

        Ref<Scene> newScene = CreateRef<Scene>();

        newScene->OnViewportResize(other->m_ViewportWidth, other->m_ViewportHeight);
        newScene->m_Skybox = other->m_Skybox;
        newScene->m_SkyboxIntensity = other->m_SkyboxIntensity;
        newScene->m_PrimaryCameraID = other->m_PrimaryCameraID;

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
    }

    Scene::~Scene()
    {
        for (auto it = m_SubsystemList.rbegin(); it != m_SubsystemList.rend(); ++it)
            (*it)->Shutdown();

        m_Subsystems.clear();
        m_SubsystemList.clear();
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        return CreateEntityWithUUID(UUID(), name);
    }

    Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
    {
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
        m_EntitiesToDestroy.push_back(entity);
    }

    void Scene::RemoveEntity(Entity entity)
    {
        OPTICK_EVENT();

        if (m_IsRunning && entity.HasComponent<ScriptComponent>())
			Application::Get().GetSubsystem<ScriptEngine>()->OnDestroyEntity(entity);

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
        OPTICK_EVENT();

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
                    if (child) nextGen.push_back((entt::entity)child);
                }
            }

            if (!nextGen.empty())
            {
                generations.push_back(nextGen);
                currentGen++;
            }
            else
            {
                break;
            }

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

    void Scene::OnUpdateSimulation(float deltaTime)
    {
        OPTICK_EVENT();

        auto physicsWorld = GetSubsystem<PhysicsWorld>();

        if (m_IsRunning)
        {
            auto scriptView = m_Registry.view<ScriptComponent>();
            std::vector<entt::entity> entities(scriptView.begin(), scriptView.end());

            if (!entities.empty())
            {
                uint32_t threadCount = Application::Get().GetSubsystem<JobSystem>()->GetThreadCount();
                uint32_t groupSize = (uint32_t)entities.size() / threadCount;
                if (groupSize == 0) groupSize = 1;

                Application::Get().GetSubsystem<JobSystem>()->Dispatch(entities.size(), groupSize, [this, &entities, deltaTime](JobDispatchArgs args)
                    {
                        OPTICK_EVENT("Run C# Fixed Update");
                        Entity entity = { entities[args.JobIndex], this };
                        Application::Get().GetSubsystem<ScriptEngine>()->OnFixedUpdateEntity(entity, deltaTime);
                    });

                Application::Get().GetSubsystem<JobSystem>()->Wait();
            }

            UpdateWorldTransforms();

            physicsWorld->LockWrite();

            auto transformView = m_Registry.view<TransformComponent>();
            for (auto e : transformView)
            {
                auto& tc = transformView.get<TransformComponent>(e);

                if (tc.IsDirty)
                {
                    Entity entity = { e, this };
                    if (entity.HasComponent<RigidbodyComponent>())
                    {
                        SyncTransformToPhysics(entity);
                    }
                    tc.IsDirty = false;
                }
            }

            physicsWorld->UnlockWrite();
        }

        auto rbView = m_Registry.view<RigidbodyComponent>();
        for (auto e : rbView)
        {
            auto& rb = rbView.get<RigidbodyComponent>(e);

            if (rb.Type != RigidbodyComponent::BodyType::Static && rb.RuntimeActor && rb.UseCCD)
            {
                physx::PxRigidDynamic* actor = (physx::PxRigidDynamic*)rb.RuntimeActor;

                float sqrVelocity = actor->getLinearVelocity().magnitudeSquared();
                float sqrThreshold = rb.CCDVelocityThreshold * rb.CCDVelocityThreshold;

                actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_CCD, sqrVelocity > sqrThreshold);
            }
        }

        for (auto& subsystem : m_SubsystemList)
            subsystem->Update(deltaTime);

        auto view = m_Registry.view<TransformComponent, RigidbodyComponent>();
        for (auto e : view)
        {
            Entity entity = { e, this };
            auto& tc = entity.GetComponent<TransformComponent>();
            auto& rb = entity.GetComponent<RigidbodyComponent>();
            auto& rc = entity.GetComponent<RelationshipComponent>();

            if (rb.Type != RigidbodyComponent::BodyType::Static && rb.RuntimeActor)
            {
                physx::PxRigidDynamic* actor = (physx::PxRigidDynamic*)rb.RuntimeActor;
                physx::PxTransform pxTransform = actor->getGlobalPose();

                glm::vec3 worldPos = PhysicsUtils::PhysXToGLM(pxTransform.p);
                glm::quat worldRot = PhysicsUtils::PhysXToGLM(pxTransform.q);

                glm::quat currentRot = glm::quat(tc.Rotation);
                bool physicsRotated = glm::abs(glm::dot(currentRot, worldRot)) < 0.9999f;

                if (rc.ParentHandle != 0)
                {
                    Entity parent = GetEntityByUUID(rc.ParentHandle);
                    if (parent)
                    {
                        glm::mat4 parentWorldTransform = GetWorldTransform(parent);
                        glm::mat4 entityWorldTransform = glm::translate(glm::mat4(1.0f), worldPos) * glm::toMat4(worldRot) * glm::scale(glm::mat4(1.0f), tc.Scale);
                        glm::mat4 localTransform = glm::inverse(parentWorldTransform) * entityWorldTransform;

                        glm::vec3 localPos, localRotEuler, localScale;
                        Math::DecomposeTransform(localTransform, localPos, localRotEuler, localScale);

                        tc.Translation = localPos;
                        if (physicsRotated)
                            tc.Rotation = localRotEuler;
                    }
                }
                else
                {
                    tc.Translation = worldPos;
                    if (physicsRotated)
                        tc.Rotation = glm::eulerAngles(worldRot);
                }
            }
        }
    }

    void Scene::OnRender(const Camera& camera, const glm::mat4& cameraTransform, Ref<RenderTarget>& renderTarget, bool showColliders)
    {
        OPTICK_EVENT();

        LightEnvironment lightEnv;
        {
            auto view = m_Registry.view<DirectionalLightComponent, TransformComponent>();
            for (auto entity : view)
            {
                auto [light, transform] = view.get<DirectionalLightComponent, TransformComponent>(entity);

                glm::vec3 direction = glm::toMat3(glm::quat(transform.Rotation)) * glm::vec3(0, 0, -1);

                lightEnv.DirLight.Direction = direction;
                lightEnv.DirLight.Color = light.Color;
                lightEnv.DirLight.Intensity = light.Intensity;
            }
        }
        {
            auto group = m_Registry.group<PointLightComponent>();
            for (auto entity : group)
            {
                auto light = group.get<PointLightComponent>(entity);
                glm::mat4 worldTransform = GetWorldTransform({ entity, this });

                PointLight pl;
                pl.Position = glm::vec3(worldTransform[3]);
                pl.Color = light.Color;
                pl.Intensity = light.Intensity;
                pl.Radius = light.Radius;
                pl.Falloff = light.Falloff;
                lightEnv.PointLights.push_back(pl);
            }
        }
        auto renderSys = Application::Get().GetSubsystem<Renderer>();
        auto jobSys = Application::Get().GetSubsystem<JobSystem>();
        auto physicsWorld = GetSubsystem<PhysicsWorld>();

        renderSys->BeginScene(camera, cameraTransform, lightEnv, m_Skybox, renderTarget);

        glm::mat4 viewProj = camera.GetProjection() * glm::inverse(cameraTransform);
        ViewFrustum frustum;
        frustum.Extract(viewProj);

        std::vector<RenderCommandData> renderQueue;
        std::mutex queueMutex;

        auto view = m_Registry.view<StaticMeshComponent, TransformComponent>();
        std::vector<entt::entity> entities(view.begin(), view.end());

        if (!entities.empty())
        {
            uint32_t threadCount = jobSys->GetThreadCount();
            uint32_t groupSize = (uint32_t)entities.size() / threadCount;
            if (groupSize == 0) groupSize = 1;

            jobSys->Dispatch(entities.size(), groupSize, [&](JobDispatchArgs args)
                {
                    entt::entity e = entities[args.JobIndex];
                    auto [mc, tc] = view.get<StaticMeshComponent, TransformComponent>(e);

                    if (!mc.Mesh) return;

                    glm::mat4 transform = tc.WorldTransform;

                    const auto& submesh = mc.Mesh->GetSubmeshes()[mc.SubmeshIndex];
                    AABB worldAABB = Math::CalculateWorldAABB(submesh.BoundingBox, transform);

                    glm::vec3 center = (worldAABB.Min + worldAABB.Max) * 0.5f;
                    float radius = glm::distance(worldAABB.Min, worldAABB.Max) * 0.5f;

                    bool isVisible = frustum.IsSphereVisible(center, radius);
                    bool isVisibleToShadows = renderSys->IsSphereVisibleToShadows(center, radius);

                    if (!isVisible && !isVisibleToShadows)
                        return;

                    RenderCommandData cmd;
                    cmd.Mesh = mc.Mesh;
                    cmd.SubmeshIndex = mc.SubmeshIndex;
                    uint32_t matIndex = mc.Mesh->GetSubmeshes()[mc.SubmeshIndex].MaterialIndex;
                    cmd.Material = mc.MaterialTableOverride ? mc.MaterialTableOverride : mc.Mesh->GetMaterials()[matIndex];
                    cmd.Transform = transform;
                    cmd.EntityID = (int)(uint32_t)e;

                    cmd.IsVisibleToCamera = isVisible;
                    cmd.IsVisibleToShadows = isVisibleToShadows;

                    {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        renderQueue.push_back(cmd);
                    }
                });

            jobSys->Wait();
        }

        for (const auto& cmd : renderQueue)
        {
            if (cmd.IsVisibleToCamera)
                renderSys->Submit(cmd.Mesh, cmd.SubmeshIndex, cmd.Material, cmd.Transform, cmd.EntityID);

            if (cmd.IsVisibleToShadows)
                renderSys->SubmitShadowCaster(cmd.Mesh, cmd.SubmeshIndex, cmd.Transform, cmd.EntityID);
        }

        if (showColliders)
        {
            if (physicsWorld->GetScene())
            {
                const physx::PxRenderBuffer& rb = physicsWorld->GetScene()->getRenderBuffer();
                for (physx::PxU32 i = 0; i < rb.getNbLines(); i++)
                {
                    const physx::PxDebugLine& line = rb.getLines()[i];
                    renderSys->DrawLine(
                        PhysicsUtils::PhysXToGLM(line.pos0),
                        PhysicsUtils::PhysXToGLM(line.pos1),
                        PhysicsUtils::UnpackPhysXColor(line.color0)
                    );
                }
            }
        }

        if (m_Skybox)
            renderSys->DrawSkybox(m_Skybox, camera, cameraTransform);

        renderSys->EndScene();
    }

    void Scene::OnRenderEditor(float deltaTime, EditorCamera& camera, Ref<RenderTarget>& renderTarget, bool showColliders)
    {
        OPTICK_EVENT();

        UpdateWorldTransforms();

        LightEnvironment lightEnv;
        {
            auto view = m_Registry.view<DirectionalLightComponent, TransformComponent>();
            for (auto entity : view)
            {
                auto [light, transform] = view.get<DirectionalLightComponent, TransformComponent>(entity);

                glm::vec3 direction = glm::toMat3(glm::quat(transform.Rotation)) * glm::vec3(0, 0, -1);

                lightEnv.DirLight.Direction = direction;
                lightEnv.DirLight.Color = light.Color;
                lightEnv.DirLight.Intensity = light.Intensity;
            }
        }
        {
            auto group = m_Registry.group<PointLightComponent>();
            for (auto entity : group)
            {
                auto light = group.get<PointLightComponent>(entity);
                glm::mat4 worldTransform = GetWorldTransform({ entity, this });

                PointLight pl;
                pl.Position = glm::vec3(worldTransform[3]);
                pl.Color = light.Color;
                pl.Intensity = light.Intensity;
                pl.Radius = light.Radius;
                pl.Falloff = light.Falloff;
                lightEnv.PointLights.push_back(pl);
            }
        }

        auto renderSys = Application::Get().GetSubsystem<Renderer>();
        auto jobSys = Application::Get().GetSubsystem<JobSystem>();

        renderSys->BeginScene(camera, lightEnv, m_Skybox, renderTarget);

        glm::mat4 viewProj = camera.GetViewProjection();
        ViewFrustum frustum;
        frustum.Extract(viewProj);

        std::vector<RenderCommandData> renderQueue;
        std::mutex queueMutex;

        auto view = m_Registry.view<StaticMeshComponent, TransformComponent>();
        std::vector<entt::entity> entities(view.begin(), view.end());

        if (!entities.empty())
        {
            uint32_t threadCount = jobSys->GetThreadCount();
            uint32_t groupSize = (uint32_t)entities.size() / threadCount;
            if (groupSize == 0) groupSize = 1;

            jobSys->Dispatch(entities.size(), groupSize, [&](JobDispatchArgs args)
                {
                    entt::entity e = entities[args.JobIndex];
                    auto [mc, tc] = view.get<StaticMeshComponent, TransformComponent>(e);

                    if (!mc.Mesh) return;

                    glm::mat4 transform = tc.WorldTransform;
                    glm::vec3 worldPos = glm::vec3(transform[3]);

                    const auto& submesh = mc.Mesh->GetSubmeshes()[mc.SubmeshIndex];
                    AABB worldAABB = Math::CalculateWorldAABB(submesh.BoundingBox, transform);

                    glm::vec3 center = (worldAABB.Min + worldAABB.Max) * 0.5f;
                    float radius = glm::distance(worldAABB.Min, worldAABB.Max) * 0.5f;

                    bool isVisible = frustum.IsSphereVisible(center, radius);
                    bool isVisibleToShadows = renderSys->IsSphereVisibleToShadows(center, radius);
                    
                    if (!isVisible && !isVisibleToShadows)
                        return;

                    RenderCommandData cmd;
                    cmd.Mesh = mc.Mesh;
                    cmd.SubmeshIndex = mc.SubmeshIndex;
                    uint32_t matIndex = mc.Mesh->GetSubmeshes()[mc.SubmeshIndex].MaterialIndex;
                    cmd.Material = mc.MaterialTableOverride ? mc.MaterialTableOverride : mc.Mesh->GetMaterials()[matIndex];
                    cmd.Transform = transform;
                    cmd.EntityID = (int)(uint32_t)e;

                    cmd.IsVisibleToCamera = isVisible;
                    cmd.IsVisibleToShadows = isVisibleToShadows;

                    {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        renderQueue.push_back(cmd);
                    }
                });

            jobSys->Wait();
        }

        for (const auto& cmd : renderQueue)
        {
            if (cmd.IsVisibleToCamera)
                renderSys->Submit(cmd.Mesh, cmd.SubmeshIndex, cmd.Material, cmd.Transform, cmd.EntityID);

            if (cmd.IsVisibleToShadows)
                renderSys->SubmitShadowCaster(cmd.Mesh, cmd.SubmeshIndex, cmd.Transform, cmd.EntityID);
        }

        if (m_Skybox)
            renderSys->DrawSkybox(m_Skybox, camera);

        if (showColliders)
        {
            {
                auto view = m_Registry.view<TransformComponent, BoxColliderComponent>();
                for (auto entity : view)
                {
                    auto [tc, bc] = view.get<TransformComponent, BoxColliderComponent>(entity);

                    glm::mat4 worldTransform = GetWorldTransform({ entity, this });
                    glm::vec3 worldTranslation, worldRotation, worldScale;
                    Math::DecomposeTransform(worldTransform, worldTranslation, worldRotation, worldScale);

                    glm::vec3 scale = worldScale * bc.HalfExtents * 2.0f;

                    glm::mat4 transform = glm::translate(glm::mat4(1.0f), worldTranslation)
                        * glm::toMat4(glm::quat(worldRotation))
                        * glm::translate(glm::mat4(1.0f), bc.Offset)
                        * glm::scale(glm::mat4(1.0f), scale);

                    renderSys->DrawWireBox(transform, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
                }
            }

            {
                auto view = m_Registry.view<TransformComponent, SphereColliderComponent>();
                for (auto entity : view)
                {
                    auto [tc, sc] = view.get<TransformComponent, SphereColliderComponent>(entity);

                    glm::mat4 worldTransform = GetWorldTransform({ entity, this });
                    glm::vec3 worldTranslation, worldRotation, worldScale;
                    Math::DecomposeTransform(worldTransform, worldTranslation, worldRotation, worldScale);

                    float maxScale = glm::max(worldScale.x, glm::max(worldScale.y, worldScale.z));
                    glm::vec3 scale = glm::vec3(sc.Radius * maxScale);

                    glm::mat4 transform = glm::translate(glm::mat4(1.0f), worldTranslation)
                        * glm::toMat4(glm::quat(worldRotation))
                        * glm::translate(glm::mat4(1.0f), sc.Offset)
                        * glm::scale(glm::mat4(1.0f), scale);

                    renderSys->DrawWireSphere(transform, glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
                }
            }

            {
                auto view = m_Registry.view<TransformComponent, CapsuleColliderComponent>();
                for (auto entity : view)
                {
                    auto [tc, cc] = view.get<TransformComponent, CapsuleColliderComponent>(entity);

                    glm::mat4 worldTransform = GetWorldTransform({ entity, this });
                    glm::vec3 worldTranslation, worldRotation, worldScale;
                    Math::DecomposeTransform(worldTransform, worldTranslation, worldRotation, worldScale);

                    float radiusScale = glm::max(worldScale.x, worldScale.z);
                    float radius = cc.Radius * radiusScale;
                    float height = cc.Height * worldScale.y;

                    glm::mat4 transform = glm::translate(glm::mat4(1.0f), worldTranslation)
                        * glm::toMat4(glm::quat(worldRotation))
                        * glm::translate(glm::mat4(1.0f), cc.Offset);

                    renderSys->DrawWireCapsule(transform, radius, height, glm::vec4(0.0f, 0.8f, 1.0f, 1.0f));
                }
            }
        }

        renderSys->EndScene();

        for (auto& entity : m_EntitiesToDestroy)
            RemoveEntity(entity);

        m_EntitiesToDestroy.clear();
    }

    void Scene::OnUpdateRuntime(float deltaTime)
    {
        OPTICK_EVENT();

        auto scriptSys = Application::Get().GetSubsystem<ScriptEngine>();
        auto jobSys = Application::Get().GetSubsystem<JobSystem>();

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
            uint32_t threadCount = jobSys->GetThreadCount();
            uint32_t groupSize = (uint32_t)entities.size() / threadCount;
            if (groupSize == 0) groupSize = 1;

            jobSys->Dispatch(entities.size(), groupSize, [this, &entities, deltaTime, &scriptSys](JobDispatchArgs args)
                {
                    OPTICK_EVENT("Run C# Update");
                    Entity entity = { entities[args.JobIndex], this };
                    scriptSys->OnUpdateEntity(entity, deltaTime);
                });

            jobSys->Wait();
        }

        for (auto& entity : m_EntitiesToDestroy)
            RemoveEntity(entity);

        m_EntitiesToDestroy.clear();

        UpdateWorldTransforms();
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
        OPTICK_EVENT();

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

            if (newEntity.HasComponent<BoxColliderComponent>()) {
                newEntity.GetComponent<BoxColliderComponent>().RuntimeShape = nullptr;
                newEntity.GetComponent<BoxColliderComponent>().RuntimeMaterial = nullptr;
            }
            if (newEntity.HasComponent<SphereColliderComponent>()) {
                newEntity.GetComponent<SphereColliderComponent>().RuntimeShape = nullptr;
                newEntity.GetComponent<SphereColliderComponent>().RuntimeMaterial = nullptr;
            }
            if (newEntity.HasComponent<CapsuleColliderComponent>()) {
                newEntity.GetComponent<CapsuleColliderComponent>().RuntimeShape = nullptr;
                newEntity.GetComponent<CapsuleColliderComponent>().RuntimeMaterial = nullptr;
            }

            if (m_IsSimulating)
            {
                CreatePhysicsBody(newEntity);
            }
        }

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
        {
            m_PrimaryCameraID = entity.GetUUID();
        }
    }

    void Scene::OnCameraComponentAdded(entt::registry& registry, entt::entity entity)
    {
        if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
        {
            auto& cameraComponent = registry.get<CameraComponent>(entity);
            if (!cameraComponent.FixedAspectRatio)
            {
                cameraComponent.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
            }
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

        m_IsRunning = true;
    }

    void Scene::OnRuntimeStop()
    {
        OnSimulationStop();
        Application::Get().GetSubsystem<ScriptEngine>()->OnRuntimeStop();

        m_IsRunning = false;
    }

    void Scene::CreatePhysicsBody(Entity entity)
    {
        OPTICK_EVENT();

        auto physicsSys = Application::Get().GetSubsystem<PhysicsSystem>();
        auto physicsWorld = GetSubsystem<PhysicsWorld>();

        physx::PxPhysics* physics = physicsSys->GetPhysics();
        physx::PxScene* physicsScene = physicsWorld->GetScene();

        auto& transform = entity.GetComponent<TransformComponent>();
        auto& rb = entity.GetComponent<RigidbodyComponent>();

        glm::mat4 worldTransform = GetWorldTransform(entity);
        glm::vec3 worldTranslation, worldRotation, worldScale;
        Math::DecomposeTransform(worldTransform, worldTranslation, worldRotation, worldScale);

        glm::quat rotation = glm::quat(worldRotation);
        physx::PxTransform pxTransform(PhysicsUtils::GLMToPhysX(worldTranslation), PhysicsUtils::GLMToPhysX(rotation));

        physx::PxRigidActor* actor = nullptr;

        if (rb.Type == RigidbodyComponent::BodyType::Static)
        {
            actor = physics->createRigidStatic(pxTransform);
        }
        else
        {
            physx::PxRigidDynamic* dynamicActor = physics->createRigidDynamic(pxTransform);

            if (rb.Type == RigidbodyComponent::BodyType::Kinematic)
                dynamicActor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);

            dynamicActor->setLinearDamping(rb.LinearDrag);
            dynamicActor->setAngularDamping(rb.AngularDrag);

            if (rb.FixedRotation)
            {
                dynamicActor->setRigidDynamicLockFlags(
                    physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X |
                    physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y |
                    physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z
                );
            }

            actor = dynamicActor;
        }

        if (entity.HasComponent<BoxColliderComponent>())
        {
            auto& bc = entity.GetComponent<BoxColliderComponent>();
            physx::PxMaterial* material = physics->createMaterial(bc.StaticFriction, bc.DynamicFriction, bc.Restitution);
            bc.RuntimeMaterial = material;

            glm::vec3 colliderSize = bc.HalfExtents * worldScale;
            physx::PxBoxGeometry boxGeom(PhysicsUtils::GLMToPhysX(colliderSize));

            physx::PxShape* shape = physx::PxRigidActorExt::createExclusiveShape(*actor, boxGeom, *material);
            shape->setLocalPose(physx::PxTransform(PhysicsUtils::GLMToPhysX(bc.Offset)));

            if (bc.IsTrigger)
            {
                shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
                shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
            }

            bc.RuntimeShape = shape;
        }

        if (entity.HasComponent<SphereColliderComponent>())
        {
            auto& sc = entity.GetComponent<SphereColliderComponent>();
            physx::PxMaterial* material = physics->createMaterial(sc.StaticFriction, sc.DynamicFriction, sc.Restitution);
            sc.RuntimeMaterial = material;

            float maxScale = glm::max(worldScale.x, glm::max(worldScale.y, worldScale.z));
            physx::PxSphereGeometry sphereGeom(sc.Radius * maxScale);

            physx::PxShape* shape = physx::PxRigidActorExt::createExclusiveShape(*actor, sphereGeom, *material);
            shape->setLocalPose(physx::PxTransform(PhysicsUtils::GLMToPhysX(sc.Offset)));

            if (sc.IsTrigger)
            {
                shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
                shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
            }

            sc.RuntimeShape = shape;
        }

        if (entity.HasComponent<CapsuleColliderComponent>())
        {
            auto& cc = entity.GetComponent<CapsuleColliderComponent>();
            physx::PxMaterial* material = physics->createMaterial(cc.StaticFriction, cc.DynamicFriction, cc.Restitution);
            cc.RuntimeMaterial = material;

            float radiusScale = glm::max(worldScale.x, worldScale.z);
            physx::PxCapsuleGeometry capsuleGeom(cc.Radius * radiusScale, (cc.Height / 2.0f) * worldScale.y);

            physx::PxShape* shape = physx::PxRigidActorExt::createExclusiveShape(*actor, capsuleGeom, *material);

            physx::PxQuat relativeRot(physx::PxHalfPi, physx::PxVec3(0.0f, 0.0f, 1.0f));

            shape->setLocalPose(physx::PxTransform(PhysicsUtils::GLMToPhysX(cc.Offset), relativeRot));

            if (cc.IsTrigger)
            {
                shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
                shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
            }

            cc.RuntimeShape = shape;
        }

        if (entity.HasComponent<MeshColliderComponent>())
        {
            auto& mc = entity.GetComponent<MeshColliderComponent>();
            auto& transform = entity.GetComponent<TransformComponent>();

            Ref<StaticMesh> collisionMesh = nullptr;

            if (!mc.OverrideAssetPath.empty())
            {
                collisionMesh = Application::Get().GetSubsystem<AssetManager>()->GetMesh(mc.OverrideAssetPath);
            }
            else
            {
                std::function<Ref<StaticMesh>(Entity)> findMesh = [&](Entity e) -> Ref<StaticMesh>
                    {
                        if (e.HasComponent<StaticMeshComponent>())
                            return e.GetComponent<StaticMeshComponent>().Mesh;

                        if (e.HasComponent<RelationshipComponent>())
                        {
                            for (UUID childID : e.GetComponent<RelationshipComponent>().Children)
                            {
                                Entity child = GetEntityByUUID(childID);
                                if (child)
                                {
                                    Ref<StaticMesh> m = findMesh(child);
                                    if (m) return m;
                                }
                            }
                        }
                        return nullptr;
                    };

                collisionMesh = findMesh(entity);
            }

            if (collisionMesh && !collisionMesh->GetVertices().empty())
            {
                physx::PxMaterial* material = physics->createMaterial(mc.StaticFriction, mc.DynamicFriction, mc.Restitution);
                mc.RuntimeMaterial = material;

                physx::PxShape* shape = nullptr;

                if (mc.IsConvex)
                {
                    physx::PxConvexMesh* convexMesh = physicsSys->CreateConvexMesh(collisionMesh);
                    if (convexMesh)
                    {
                        physx::PxMeshScale scale(physx::PxVec3(worldScale.x, worldScale.y, worldScale.z), physx::PxQuat(physx::PxIdentity));
                        physx::PxConvexMeshGeometry geom(convexMesh, scale);

                        shape = physx::PxRigidActorExt::createExclusiveShape(*actor, geom, *material);
                    }
                }
                else
                {
                    physx::PxTriangleMesh* triMesh = physicsSys->CreateTriangleMesh(collisionMesh);
                    if (triMesh)
                    {
                        physx::PxMeshScale scale(physx::PxVec3(worldScale.x, worldScale.y, worldScale.z), physx::PxQuat(physx::PxIdentity));
                        physx::PxTriangleMeshGeometry geom(triMesh, scale);

                        shape = physx::PxRigidActorExt::createExclusiveShape(*actor, geom, *material);
                    }
                }

                if (shape)
                {
                    if (mc.IsTrigger)
                    {
                        shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
                        shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
                    }
                    mc.RuntimeShape = shape;
                }
            }
            else
            {
                RXN_CORE_WARN("MeshColliderComponent on Entity '{0}' failed to build: Mesh not loaded yet or no child mesh found.", entity.GetComponent<TagComponent>().Tag);
            }
        }

        if (rb.Type != RigidbodyComponent::BodyType::Static)
        {
            physx::PxRigidBodyExt::updateMassAndInertia(*(physx::PxRigidDynamic*)actor, rb.Mass);
        }

        actor->userData = (void*)(uint64_t)entity.GetUUID();

        physicsScene->addActor(*actor);
        rb.RuntimeActor = actor;
    }

    void Scene::OnSimulationStart()
    {
        m_Registry.view<entt::entity>().each([&](auto entityID)
            {
                Entity entity = { entityID, this };

                bool hasCollider = entity.HasComponent<BoxColliderComponent>() ||
                    entity.HasComponent<SphereColliderComponent>() ||
                    entity.HasComponent<CapsuleColliderComponent>() ||
                    entity.HasComponent<MeshColliderComponent>();

                if (hasCollider && !entity.HasComponent<RigidbodyComponent>())
                {
                    auto& rb = entity.AddComponent<RigidbodyComponent>();
                    rb.Type = RigidbodyComponent::BodyType::Static;
                }

                if (entity.HasComponent<RigidbodyComponent>())
                    CreatePhysicsBody(entity);
            });

        m_IsSimulating = true;
    }

    void Scene::OnSimulationStop()
    {
        m_IsSimulating = false;
    }

    void Scene::SyncTransformToPhysics(Entity entity)
    {
        if (!entity.HasComponent<RigidbodyComponent>()) return;

        auto& rb = entity.GetComponent<RigidbodyComponent>();
        auto& tc = entity.GetComponent<TransformComponent>();

        if (rb.RuntimeActor)
        {
            physx::PxRigidActor* actor = static_cast<physx::PxRigidActor*>(rb.RuntimeActor);

            glm::quat rotation = glm::quat(tc.Rotation);

            physx::PxTransform physxTransform(
                physx::PxVec3(tc.Translation.x, tc.Translation.y, tc.Translation.z),
                physx::PxQuat(rotation.x, rotation.y, rotation.z, rotation.w)
            );

            actor->setGlobalPose(physxTransform);
        }
    }
}