#include "rxnpch.h"
#include "PhysicsWorld.h"
#include "PhysicsSystem.h"
#include "RXNEngine/Scene/Scene.h"
#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Scene/Components.h"
#include "RXNEngine/Math/Math.h"
#include "RXNEngine/Asset/AssetManager.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace RXNEngine {

    namespace PhysicsUtils {
        inline physx::PxVec3 GLMToPhysX(const glm::vec3& vec) { return { vec.x, vec.y, vec.z }; }
        inline glm::vec3 PhysXToGLM(const physx::PxVec3& vec) { return { vec.x, vec.y, vec.z }; }

        inline physx::PxQuat GLMToPhysX(const glm::quat& q) { return { q.x, q.y, q.z, q.w }; }
        inline glm::quat PhysXToGLM(const physx::PxQuat& q) { return { q.w, q.x, q.y, q.z }; }
    }

    physx::PxFilterFlags ContactReportFilterShader(
        physx::PxFilterObjectAttributes attributes0, physx::PxFilterData filterData0,
        physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1,
        physx::PxPairFlags& pairFlags, const void* constantBlock, physx::PxU32 constantBlockSize)
    {
        pairFlags = physx::PxPairFlag::eCONTACT_DEFAULT;
        pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND | physx::PxPairFlag::eNOTIFY_TOUCH_LOST;
        pairFlags |= physx::PxPairFlag::eDETECT_CCD_CONTACT;
        return physx::PxFilterFlag::eDEFAULT;
    }

    void PhysicsWorld::Init()
    {
        auto physicsSys = Application::Get().GetSubsystem<PhysicsSystem>();
        physx::PxPhysics* physics = physicsSys->GetPhysics();

        physx::PxSceneDesc sceneDesc(physics->getTolerancesScale());
        sceneDesc.cpuDispatcher = physicsSys->GetDispatcher();
        sceneDesc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
        sceneDesc.filterShader = ContactReportFilterShader;
        sceneDesc.simulationEventCallback = &m_ContactListener;
        sceneDesc.flags |= physx::PxSceneFlag::eENABLE_CCD;
        sceneDesc.flags |= physx::PxSceneFlag::eREQUIRE_RW_LOCK;

        m_Scene = physics->createScene(sceneDesc);
        m_Scene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 1.0f);
        m_Scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);

        m_ControllerManager = PxCreateControllerManager(*m_Scene);
    }

    void PhysicsWorld::Update(float deltaTime)
    {
        RXN_PROFILE_SCOPE();

        if (!m_Scene) 
            return;

        m_Scene->simulate(deltaTime);
        m_Scene->fetchResults(true);
    }

    void PhysicsWorld::Shutdown()
    {
        if (m_ControllerManager)
            m_ControllerManager->release();

        if (m_Scene)
            m_Scene->release();

        m_ControllerManager = nullptr;
        m_Scene = nullptr;
    }

    void PhysicsWorld::CreatePhysicsBody(Entity entity)
    {
        RXN_PROFILE_SCOPE();

        auto physicsSys = Application::Get().GetSubsystem<PhysicsSystem>();
        physx::PxPhysics* physics = physicsSys->GetPhysics();

        auto& rb = entity.GetComponent<RigidbodyComponent>();

        glm::mat4 worldTransform = entity.GetScene()->GetWorldTransform(entity);
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

            float staticFric = bc.PhysicsMaterialAsset ? bc.PhysicsMaterialAsset->StaticFriction : 0.5f;
            float dynFric = bc.PhysicsMaterialAsset ? bc.PhysicsMaterialAsset->DynamicFriction : 0.5f;
            float rest = bc.PhysicsMaterialAsset ? bc.PhysicsMaterialAsset->Restitution : 0.1f;

            physx::PxMaterial* material = physics->createMaterial(staticFric, dynFric, rest);
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

            float staticFric = sc.PhysicsMaterialAsset ? sc.PhysicsMaterialAsset->StaticFriction : 0.5f;
            float dynFric = sc.PhysicsMaterialAsset ? sc.PhysicsMaterialAsset->DynamicFriction : 0.5f;
            float rest = sc.PhysicsMaterialAsset ? sc.PhysicsMaterialAsset->Restitution : 0.1f;

            physx::PxMaterial* material = physics->createMaterial(staticFric, dynFric, rest);
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

            float staticFric = cc.PhysicsMaterialAsset ? cc.PhysicsMaterialAsset->StaticFriction : 0.5f;
            float dynFric = cc.PhysicsMaterialAsset ? cc.PhysicsMaterialAsset->DynamicFriction : 0.5f;
            float rest = cc.PhysicsMaterialAsset ? cc.PhysicsMaterialAsset->Restitution : 0.1f;

            physx::PxMaterial* material = physics->createMaterial(staticFric, dynFric, rest);
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
                                Entity child = entity.GetScene()->GetEntityByUUID(childID);
                                if (child)
                                {
                                    Ref<StaticMesh> m = findMesh(child);

                                    if (m)
                                        return m;
                                }
                            }
                        }
                        return nullptr;
                    };

                collisionMesh = findMesh(entity);
            }

            if (collisionMesh && !collisionMesh->GetVertices().empty())
            {
                float staticFric = mc.PhysicsMaterialAsset ? mc.PhysicsMaterialAsset->StaticFriction : 0.5f;
                float dynFric = mc.PhysicsMaterialAsset ? mc.PhysicsMaterialAsset->DynamicFriction : 0.5f;
                float rest = mc.PhysicsMaterialAsset ? mc.PhysicsMaterialAsset->Restitution : 0.1f;

                physx::PxMaterial* material = physics->createMaterial(staticFric, dynFric, rest);
                mc.RuntimeMaterial = material;

                physx::PxShape* shape = nullptr;

                if (mc.IsConvex)
                {
                    bool builtCompound = false;

                    for (const auto& submesh : collisionMesh->GetSubmeshes())
                    {
                        if (!submesh.ConvexHulls.empty())
                        {
                            builtCompound = true;
                            for (const auto& hull : submesh.ConvexHulls)
                            {
                                std::vector<glm::vec3> worldHullVertices(hull.Vertices.size());
                                for (size_t i = 0; i < hull.Vertices.size(); i++)
                                    worldHullVertices[i] = glm::vec3(submesh.LocalTransform * glm::vec4(hull.Vertices[i], 1.0f));

                                physx::PxConvexMesh* convexMesh = physicsSys->CreateConvexMesh(worldHullVertices);
                                if (convexMesh)
                                {
                                    physx::PxMeshScale scale(physx::PxVec3(worldScale.x, worldScale.y, worldScale.z), physx::PxQuat(physx::PxIdentity));
                                    physx::PxConvexMeshGeometry geom(convexMesh, scale);

                                    shape = physx::PxRigidActorExt::createExclusiveShape(*actor, geom, *material);

                                    if (mc.IsTrigger)
                                    {
                                        shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
                                        shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
                                    }
                                    mc.RuntimeShape = shape;
                                }
                            }
                        }
                    }

                    if (!builtCompound)
                    {
                        physx::PxConvexMesh* convexMesh = physicsSys->CreateConvexMesh(collisionMesh);
                        if (convexMesh)
                        {
                            physx::PxMeshScale scale(physx::PxVec3(worldScale.x, worldScale.y, worldScale.z), physx::PxQuat(physx::PxIdentity));
                            physx::PxConvexMeshGeometry geom(convexMesh, scale);

                            shape = physx::PxRigidActorExt::createExclusiveShape(*actor, geom, *material);
                            if (mc.IsTrigger)
                            {
                                shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
                                shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
                            }
                            mc.RuntimeShape = shape;
                        }
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
                RXN_CORE_WARN("MeshColliderComponent on Entity '{0}' failed to build physics representation.", entity.GetComponent<TagComponent>().Tag);
            }
        }

        if (rb.Type != RigidbodyComponent::BodyType::Static)
            physx::PxRigidBodyExt::updateMassAndInertia(*(physx::PxRigidDynamic*)actor, rb.Mass);

        actor->userData = (void*)(uint64_t)entity.GetUUID();
        m_Scene->addActor(*actor);
        rb.RuntimeActor = actor;
    }

    void PhysicsWorld::DestroyPhysicsBody(Entity entity)
    {
        if (entity.HasComponent<RigidbodyComponent>())
        {
            auto& rb = entity.GetComponent<RigidbodyComponent>();
            if (rb.RuntimeActor)
            {
                physx::PxRigidActor* actor = (physx::PxRigidActor*)rb.RuntimeActor;
                if (m_Scene)
                    m_Scene->removeActor(*actor);

                actor->release();
                rb.RuntimeActor = nullptr;
            }
        }

        if (entity.HasComponent<BoxColliderComponent>())
        {
            auto& bc = entity.GetComponent<BoxColliderComponent>();
            bc.RuntimeShape = nullptr;
            if (bc.RuntimeMaterial)
            {
                ((physx::PxMaterial*)bc.RuntimeMaterial)->release();
                bc.RuntimeMaterial = nullptr;
            }
        }

        if (entity.HasComponent<SphereColliderComponent>())
        {
            auto& sc = entity.GetComponent<SphereColliderComponent>();
            sc.RuntimeShape = nullptr;
            if (sc.RuntimeMaterial)
            {
                ((physx::PxMaterial*)sc.RuntimeMaterial)->release();
                sc.RuntimeMaterial = nullptr;
            }
        }

        if (entity.HasComponent<CapsuleColliderComponent>())
        {
            auto& cc = entity.GetComponent<CapsuleColliderComponent>();
            cc.RuntimeShape = nullptr;
            if (cc.RuntimeMaterial)
            { 
                ((physx::PxMaterial*)cc.RuntimeMaterial)->release();
                cc.RuntimeMaterial = nullptr;
            }
        }

        if (entity.HasComponent<MeshColliderComponent>())
        {
            auto& mc = entity.GetComponent<MeshColliderComponent>();
            mc.RuntimeShape = nullptr;
            if (mc.RuntimeMaterial)
            { 
                ((physx::PxMaterial*)mc.RuntimeMaterial)->release();
                mc.RuntimeMaterial = nullptr;
            }
        }

        if (entity.HasComponent<CharacterControllerComponent>())
        {
            auto& cct = entity.GetComponent<CharacterControllerComponent>();
            if (cct.RuntimeController)
            {
                ((physx::PxController*)cct.RuntimeController)->release();
                cct.RuntimeController = nullptr;
            }
        }
    }

    void PhysicsWorld::SyncTransformToPhysics(Entity entity)
    {
        if (!entity.HasComponent<RigidbodyComponent>())
            return;

        auto& rb = entity.GetComponent<RigidbodyComponent>();
        if (rb.RuntimeActor)
        {
            physx::PxRigidActor* actor = static_cast<physx::PxRigidActor*>(rb.RuntimeActor);

            glm::mat4 worldTransform = entity.GetScene()->GetWorldTransform(entity);
            glm::vec3 worldTranslation, worldRotation, worldScale;
            Math::DecomposeTransform(worldTransform, worldTranslation, worldRotation, worldScale);

            glm::quat rotation = glm::quat(worldRotation);
            physx::PxTransform physxTransform(
                physx::PxVec3(worldTranslation.x, worldTranslation.y, worldTranslation.z),
                physx::PxQuat(rotation.x, rotation.y, rotation.z, rotation.w)
            );

            actor->setGlobalPose(physxTransform);
        }
    }

    void PhysicsWorld::OnSimulationStart(Scene* scene)
    {
        std::vector<entt::entity> entitiesNeedingStaticRB;

        scene->GetRaw().view<entt::entity>().each([&](auto entityID)
            {
                Entity entity = { entityID, scene };

                bool hasCollider = entity.HasComponent<BoxColliderComponent>() ||
                    entity.HasComponent<SphereColliderComponent>() ||
                    entity.HasComponent<CapsuleColliderComponent>() ||
                    entity.HasComponent<MeshColliderComponent>();

                if (hasCollider && !entity.HasComponent<RigidbodyComponent>())
                    entitiesNeedingStaticRB.push_back(entityID);
            });

        for (auto entityID : entitiesNeedingStaticRB)
        {
            Entity entity = { entityID, scene };
            auto& rb = entity.AddComponent<RigidbodyComponent>();
            rb.Type = RigidbodyComponent::BodyType::Static;
        }

        scene->GetRaw().view<entt::entity>().each([&](auto entityID)
            {
                Entity entity = { entityID, scene };

                if (entity.HasComponent<RigidbodyComponent>())
                    CreatePhysicsBody(entity);

                if (entity.HasComponent<CharacterControllerComponent>())
                {
                    auto& cct = entity.GetComponent<CharacterControllerComponent>();
                    auto& tc = entity.GetComponent<TransformComponent>();

                    physx::PxCapsuleControllerDesc desc;
                    desc.setToDefault();
                    desc.height = cct.Height;
                    desc.radius = cct.Radius;
                    desc.stepOffset = cct.StepOffset;
                    desc.slopeLimit = glm::cos(glm::radians(cct.SlopeLimitDegrees));
                    desc.position = physx::PxExtendedVec3(tc.Translation.x, tc.Translation.y, tc.Translation.z);
                    desc.upDirection = physx::PxVec3(0, 1, 0);

                    auto physicsSys = Application::Get().GetSubsystem<PhysicsSystem>();
                    desc.material = physicsSys->GetPhysics()->createMaterial(0.5f, 0.5f, 0.1f);

                    cct.RuntimeController = m_ControllerManager->createController(desc);

                    if (cct.RuntimeController)
                    {
                        physx::PxController* pxCct = (physx::PxController*)cct.RuntimeController;
                        pxCct->setUserData((void*)(uint64_t)entity.GetUUID());
                        pxCct->getActor()->userData = (void*)(uint64_t)entity.GetUUID();
                    }
                }
            });
    }

    void PhysicsWorld::OnSimulationStop(Scene* scene)
    {
        if (!scene)
            return;

        scene->GetRaw().view<entt::entity>().each([&](auto entityID)
            {
                Entity entity = { entityID, scene };
                DestroyPhysicsBody(entity);
            });
    }

    void PhysicsWorld::UpdateCCDFlags(Scene* scene)
    {
        auto rbView = scene->GetRaw().view<RigidbodyComponent>();
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
    }

    void PhysicsWorld::SyncPhysicsToTransforms(Scene* scene)
    {
        auto view = scene->GetRaw().view<TransformComponent, RigidbodyComponent>();
        for (auto e : view)
        {
            Entity entity = { e, scene };
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
                    Entity parent = scene->GetEntityByUUID(rc.ParentHandle);
                    if (parent)
                    {
                        glm::mat4 parentWorldTransform = scene->GetWorldTransform(parent);
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

        auto cctView = scene->GetRaw().view<TransformComponent, CharacterControllerComponent>();
        for (auto e : cctView)
        {
            auto [tc, cct] = cctView.get<TransformComponent, CharacterControllerComponent>(e);
            if (cct.RuntimeController)
            {
                physx::PxController* controller = (physx::PxController*)cct.RuntimeController;
                const physx::PxExtendedVec3& pos = controller->getPosition();
                tc.Translation = glm::vec3((float)pos.x, (float)pos.y, (float)pos.z);
            }
        }
    }
}