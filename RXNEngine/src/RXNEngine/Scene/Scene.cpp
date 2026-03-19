#include "rxnpch.h"
#include "Scene.h"
#include "Components.h"
#include "Entity.h"
#include "RXNEngine/Renderer/Renderer.h"
#include "RXNEngine/Physics/PhysicsSystem.h"
#include "RXNEngine/Scripting/ScriptEngine.h"

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
        Entity entity = { m_Registry.create(), this };
        entity.AddComponent<IDComponent>(uuid);
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<RelationshipComponent>();
        auto& tag = entity.AddComponent<TagComponent>();
        tag.Tag = name.empty() ? "Entity" : name;

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
        {
            ScriptEngine::OnDestroyEntity(entity);
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

    glm::mat4 Scene::GetWorldTransform(Entity entity)
    {
        OPTICK_EVENT();

        glm::mat4 transform = entity.GetComponent<TransformComponent>().GetTransform();
        auto& rc = entity.GetComponent<RelationshipComponent>();

        if (rc.ParentHandle != 0)
        {
            Entity parent = GetEntityByUUID(rc.ParentHandle);
            if (parent)
                transform = GetWorldTransform(parent) * transform;
        }
        return transform;
    }

    void Scene::OnUpdateSimulation(float deltaTime)
    {
        OPTICK_EVENT();

        if (m_IsRunning)
        {
            auto scriptView = m_Registry.view<ScriptComponent>();
            for (auto e : scriptView)
            {
                Entity entity = { e, this };
                ScriptEngine::OnFixedUpdateEntity(entity, deltaTime);
            }
        }

        PhysicsSystem::Update(deltaTime);
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

        Renderer::BeginScene(camera, cameraTransform, lightEnv, m_Skybox, renderTarget);

        auto view = m_Registry.view<StaticMeshComponent>();
        for (auto entity : view)
        {
            auto mc = view.get<StaticMeshComponent>(entity);

            if (mc.Mesh)
            {
                uint32_t materialIndex = mc.Mesh->GetSubmeshes()[mc.SubmeshIndex].MaterialIndex;

                Ref<Material> material = mc.MaterialTableOverride ?
                    mc.MaterialTableOverride :
                    mc.Mesh->GetMaterials()[materialIndex];

                Renderer::Submit(mc.Mesh, mc.SubmeshIndex, material, GetWorldTransform({ entity, this }));
            }
        }

        if (showColliders)
        {
            if (PhysicsSystem::GetScene())
            {
                const physx::PxRenderBuffer& rb = PhysicsSystem::GetScene()->getRenderBuffer();
                for (physx::PxU32 i = 0; i < rb.getNbLines(); i++)
                {
                    const physx::PxDebugLine& line = rb.getLines()[i];
                    Renderer::DrawLine(
                        PhysicsUtils::PhysXToGLM(line.pos0),
                        PhysicsUtils::PhysXToGLM(line.pos1),
                        PhysicsUtils::UnpackPhysXColor(line.color0)
                    );
                }
            }
        }

        if (m_Skybox)
            Renderer::DrawSkybox(m_Skybox, camera, cameraTransform);

        Renderer::EndScene();

    }

    void Scene::OnRenderEditor(float deltaTime, EditorCamera& camera, Ref<RenderTarget>& renderTarget, bool showColliders)
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

        Renderer::BeginScene(camera, lightEnv, m_Skybox, renderTarget);

        auto view = m_Registry.view<StaticMeshComponent>();
        for (auto entity : view)
        {
            auto mc = view.get<StaticMeshComponent>(entity);

            if (mc.Mesh)
            {
                uint32_t materialIndex = mc.Mesh->GetSubmeshes()[mc.SubmeshIndex].MaterialIndex;

                Ref<Material> material = mc.MaterialTableOverride ?
                    mc.MaterialTableOverride :
                    mc.Mesh->GetMaterials()[materialIndex];

                Renderer::Submit(mc.Mesh, mc.SubmeshIndex, material, GetWorldTransform({ entity, this }), (int)(uint32_t)entity);
            }
        }

        if (m_Skybox)
            Renderer::DrawSkybox(m_Skybox, camera);

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

                    Renderer::DrawWireBox(transform, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
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

                    Renderer::DrawWireSphere(transform, glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
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

                    Renderer::DrawWireCapsule(transform, radius, height, glm::vec4(0.0f, 0.8f, 1.0f, 1.0f));
                }
            }
        }

        Renderer::EndScene();

        for (auto& entity : m_EntitiesToDestroy)
        {
            RemoveEntity(entity);
        }
        m_EntitiesToDestroy.clear();
    }

    void Scene::OnUpdateRuntime(float deltaTime)
    {
        OPTICK_EVENT();

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

        auto view = m_Registry.view<ScriptComponent>();
        for (auto e : view)
        {
            Entity entity = { e, this };
            ScriptEngine::OnUpdateEntity(entity, deltaTime);
        }

        for (auto& entity : m_EntitiesToDestroy)
        {
            RemoveEntity(entity);
        }
        m_EntitiesToDestroy.clear();
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
            {
                cameraComponent.Camera.SetViewportSize(width, height);
            }
        }
    }

    Entity Scene::GetEntityByUUID(UUID uuid)
    {
        auto view = m_Registry.view<IDComponent>();
        for (auto entity : view)
        {
            const auto& id = view.get<IDComponent>(entity);
            if (id.ID == uuid)
                return { entity, this };
        }
        return {};
    }

    Entity Scene::DuplicateEntity(Entity entity)
    {
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
                ScriptEngine::OnCreateEntity(newEntity);
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
        ScriptEngine::OnRuntimeStart(this);

        auto view = m_Registry.view<ScriptComponent>();
        for (auto e : view)
        {
            Entity entity = { e, this };
            ScriptEngine::OnCreateEntity(entity);
        }

        m_IsRunning = true;
    }

    void Scene::OnRuntimeStop()
    {
        OnSimulationStop();
        ScriptEngine::OnRuntimeStop();

        m_IsRunning = false;
    }

    void Scene::CreatePhysicsBody(Entity entity)
    {
        physx::PxPhysics* physics = PhysicsSystem::GetPhysics();
        physx::PxScene* physicsScene = PhysicsSystem::GetScene();

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
        PhysicsSystem::CreateScene();

        auto view = m_Registry.view<TransformComponent, RigidbodyComponent>();
        for (auto e : view)
        {
            CreatePhysicsBody({ e, this });
        }

        m_IsSimulating = true;
    }

    void Scene::OnSimulationStop()
    {
        PhysicsSystem::DestroyScene();
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