#include "rxnpch.h"
#include "ScriptInterop.h"
#include "ScriptEngine.h"
#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Core/Input.h"
#include "RXNEngine/Core/KeyCodes.h"
#include "RXNEngine/Core/Application.h"
#include "RXNEngine/Physics/PhysicsSystem.h"

#include <coreclr_delegates.h>
#include <PxPhysicsAPI.h>

namespace RXNEngine {

    namespace {
        class IgnoreEntityFilter : public physx::PxQueryFilterCallback
        {
        public:
            uint64_t IgnoreID;
            IgnoreEntityFilter(uint64_t id) : IgnoreID(id) {}

            virtual physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData& filterData, const physx::PxShape* shape, const physx::PxRigidActor* actor, physx::PxHitFlags& queryFlags) override
            {
                if (actor && actor->userData && (uint64_t)actor->userData == IgnoreID)
                    return physx::PxQueryHitType::eNONE;

                return physx::PxQueryHitType::eBLOCK;
            }

            virtual physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData& filterData, const physx::PxQueryHit& hit, const physx::PxShape* shape, const physx::PxRigidActor* actor) override
            {
                return physx::PxQueryHitType::eBLOCK;
            }
        };
    }

#pragma region Logging & Core
    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeLogMessage(const char* message)
    {
        RXN_CORE_WARN("C# SAYS: {0}", message);
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeScriptField_Register(const char* className, const char* fieldName, uint32_t type)
    {
        ScriptEngine::RegisterField(className, fieldName, (ScriptFieldType)type);
    }
#pragma endregion

#pragma region Input
    extern "C" uint8_t CORECLR_DELEGATE_CALLTYPE NativeInput_IsKeyDown(uint32_t keycode)
    {
        return Input::IsKeyPressed((KeyCode)keycode) ? 1 : 0;
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeInput_GetMousePosition(glm::vec2* outPosition)
    {
        float x = Input::GetMouseX();
        float y = Input::GetMouseY();
        *outPosition = glm::vec2(x, y);
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeInput_SetCursorMode(int mode)
    {
        Application::Get().GetWindow().SetCursorMode((CursorMode)mode);
    }
#pragma endregion

#pragma region Entity Lifecycle
    extern "C" uint64_t CORECLR_DELEGATE_CALLTYPE NativeEntity_Create()
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        if (!scene) return 0;

        Entity newEntity = scene->CreateEntity("Spawned Entity");
        return newEntity.GetUUID();
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeEntity_Destroy(uint64_t entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        if (!scene) return;

        Entity entity = scene->GetEntityByUUID(entityID);
        if (entity)
        {
            scene->DestroyEntity(entity);
        }
    }

    extern "C" uint64_t CORECLR_DELEGATE_CALLTYPE NativeEntity_FindByName(const char* name)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        if (!scene) return 0;

        Entity entity = scene->FindEntityByName(name);
        return entity ? entity.GetUUID() : UUID::Null;
    }

    extern "C" uint64_t CORECLR_DELEGATE_CALLTYPE NativeEntity_InstantiatePrefab(uint64_t entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        if (!scene) return 0;

        Entity original = scene->GetEntityByUUID(entityID);
        if (!original) return 0;

        Entity newEntity = scene->DuplicateEntity(original);
        return newEntity.GetUUID();
    }
#pragma endregion

#pragma region Transform Math

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeEntity_GetWorldPosition(uint64_t entityID, glm::vec3* outPosition)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity) return;

        glm::mat4 transform = scene->GetWorldTransform(entity);
        *outPosition = glm::vec3(transform[3]);
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeEntity_GetTranslation(uint64_t entityID, glm::vec3* outTranslation)
    {
        Scene* scene = ScriptEngine::GetSceneContext();

        RXN_CORE_ASSERT(scene, "No active scene!");
        Entity entity = scene->GetEntityByUUID(entityID);

        if (!entity)
        {
            RXN_CORE_ERROR("ScriptInterop: Tried to Get Translation of invalid Entity {0}!", entityID);
            return;
        }

        *outTranslation = entity.GetComponent<TransformComponent>().Translation;
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeEntity_SetTranslation(uint64_t entityID, glm::vec3* inTranslation)
    {
        Scene* scene = ScriptEngine::GetSceneContext();

        RXN_CORE_ASSERT(scene, "No active scene!");
        Entity entity = scene->GetEntityByUUID(entityID);

        if (!entity)
        {
            RXN_CORE_ERROR("ScriptInterop: Tried to Set Translation of invalid Entity {0}!", entityID);
            return;
        }

        entity.GetComponent<TransformComponent>().Translation = *inTranslation;

        if (entity.HasComponent<RigidbodyComponent>())
            scene->SyncTransformToPhysics(entity);
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeEntity_GetRotation(uint64_t entityID, glm::vec3* outRotation)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity) return;

        *outRotation = entity.GetComponent<TransformComponent>().Rotation;
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeEntity_SetRotation(uint64_t entityID, glm::vec3* inRotation)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity) return;

        entity.GetComponent<TransformComponent>().Rotation = *inRotation;

        scene->SyncTransformToPhysics(entity);
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeEntity_GetForward(uint64_t entityID, glm::vec3* outForward)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity) return;

        glm::mat4 transform = scene->GetWorldTransform(entity);
        *outForward = glm::normalize(glm::vec3(transform * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeEntity_GetRight(uint64_t entityID, glm::vec3* outRight)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity) return;

        glm::mat4 transform = scene->GetWorldTransform(entity);
        *outRight = glm::normalize(glm::vec3(transform * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)));
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeEntity_GetUp(uint64_t entityID, glm::vec3* outUp)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity) return;

        glm::mat4 transform = scene->GetWorldTransform(entity);
        *outUp = glm::normalize(glm::vec3(transform * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));
    }
#pragma endregion

#pragma region Physics Interop
    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeRigidbody_ApplyLinearImpulse(uint64_t entityID, glm::vec3* impulse, uint8_t wakeUp)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        if (!scene) return;

        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<RigidbodyComponent>())
            return;

        auto& rb = entity.GetComponent<RigidbodyComponent>();
        if (rb.RuntimeActor)
        {
            physx::PxRigidDynamic* dynamicActor = static_cast<physx::PxRigidActor*>(rb.RuntimeActor)->is<physx::PxRigidDynamic>();
            if (dynamicActor)
            {
                dynamicActor->addForce(
                    physx::PxVec3(impulse->x, impulse->y, impulse->z),
                    physx::PxForceMode::eIMPULSE,
                    wakeUp != 0
                );
            }
        }
    }

    extern "C" uint8_t CORECLR_DELEGATE_CALLTYPE NativePhysics_Raycast(glm::vec3* origin, glm::vec3* direction, float maxDistance, RaycastHit* outHit, uint64_t ignoreEntityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        if (!scene) return 0;

        physx::PxScene* pxScene = PhysicsSystem::GetScene();
        if (!pxScene) return 0;

        physx::PxVec3 pxOrigin(origin->x, origin->y, origin->z);
        physx::PxVec3 pxDir(direction->x, direction->y, direction->z);
        pxDir.normalize();

        physx::PxRaycastBuffer hitInfo;

        IgnoreEntityFilter filter(ignoreEntityID);
        physx::PxQueryFilterData filterData;
        filterData.flags |= physx::PxQueryFlag::ePREFILTER;

        bool hit = pxScene->raycast(pxOrigin, pxDir, maxDistance, hitInfo, physx::PxHitFlags(physx::PxHitFlag::eDEFAULT), filterData, &filter);

        if (hit && hitInfo.hasBlock)
        {
            physx::PxRaycastHit block = hitInfo.block;

            if (block.actor && block.actor->userData)
            {
                outHit->EntityID = (uint64_t)block.actor->userData;
                outHit->Position = glm::vec3(block.position.x, block.position.y, block.position.z);
                outHit->Normal = glm::vec3(block.normal.x, block.normal.y, block.normal.z);
                outHit->Distance = block.distance;

                return 1;
            }
        }

        return 0;
    }

#pragma endregion

#pragma region Component Accessors
    extern "C" uint8_t CORECLR_DELEGATE_CALLTYPE NativeEntity_HasComponent(uint64_t entityID, const char* componentType)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity) return 0;

        std::string_view type(componentType);

        if (type == "IDComponent") return entity.HasComponent<IDComponent>() ? 1 : 0;
        if (type == "TagComponent") return entity.HasComponent<TagComponent>() ? 1 : 0;
        if (type == "TransformComponent") return entity.HasComponent<TransformComponent>() ? 1 : 0;
        if (type == "RelationshipComponent") return entity.HasComponent<RelationshipComponent>() ? 1 : 0;
        if (type == "StaticMeshComponent") return entity.HasComponent<StaticMeshComponent>() ? 1 : 0;
        if (type == "CameraComponent") return entity.HasComponent<CameraComponent>() ? 1 : 0;
        if (type == "DirectionalLightComponent") return entity.HasComponent<DirectionalLightComponent>() ? 1 : 0;
        if (type == "PointLightComponent") return entity.HasComponent<PointLightComponent>() ? 1 : 0;
        if (type == "ScriptComponent") return entity.HasComponent<ScriptComponent>() ? 1 : 0;
        if (type == "RigidbodyComponent") return entity.HasComponent<RigidbodyComponent>() ? 1 : 0;
        if (type == "BoxColliderComponent") return entity.HasComponent<BoxColliderComponent>() ? 1 : 0;
        if (type == "SphereColliderComponent") return entity.HasComponent<SphereColliderComponent>() ? 1 : 0;
        if (type == "CapsuleColliderComponent") return entity.HasComponent<CapsuleColliderComponent>() ? 1 : 0;

        return 0;
    }


    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeTag_Get(uint64_t entityID, char* outBuffer, uint32_t maxLength)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        std::string tag = entity.GetComponent<TagComponent>().Tag;

        strncpy_s(outBuffer, maxLength, tag.c_str(), _TRUNCATE);
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeTag_Set(uint64_t entityID, const char* inTag)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        entity.GetComponent<TagComponent>().Tag = inTag;
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeTransform_Get(uint64_t entityID, TransformComponent* outData)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        *outData = entity.GetComponent<TransformComponent>();
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeTransform_Set(uint64_t entityID, TransformComponent* inData)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);

        entity.GetComponent<TransformComponent>() = *inData;

        if (entity.HasComponent<RigidbodyComponent>())
            scene->SyncTransformToPhysics(entity);
    }

    extern "C" uint64_t CORECLR_DELEGATE_CALLTYPE NativeRelationship_GetParent(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        return entity.GetComponent<RelationshipComponent>().ParentHandle;
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeRelationship_SetParent(uint64_t entityID, uint64_t parentID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        entity.GetComponent<RelationshipComponent>().ParentHandle = parentID;
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeStaticMesh_GetAssetPath(uint64_t entityID, char* outBuffer, uint32_t maxLength)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        std::string tag = entity.GetComponent<StaticMeshComponent>().AssetPath;

        strncpy_s(outBuffer, maxLength, tag.c_str(), _TRUNCATE);
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeStaticMesh_SetAssetPath(uint64_t entityID, const char* inTag)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        entity.GetComponent<StaticMeshComponent>().AssetPath = inTag;
    }
     
    //CameraComponent

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeDirLight_Get(uint64_t entityID, DirectionalLightComponent* outComponent)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        *outComponent = entity.GetComponent<DirectionalLightComponent>();
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeDirLight_Set(uint64_t entityID, DirectionalLightComponent* inComponent)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        entity.GetComponent<DirectionalLightComponent>() = *inComponent;
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativePointLight_Get(uint64_t entityID, PointLightComponent* outComponent)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        *outComponent = entity.GetComponent<PointLightComponent>();
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativePointLight_Set(uint64_t entityID, PointLightComponent* inComponent)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        entity.GetComponent<PointLightComponent>() = *inComponent;
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeScript_Get(uint64_t entityID, char* outBuffer, uint32_t maxLength)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        std::string scriptPath = entity.GetComponent<ScriptComponent>().ClassName;

        strncpy_s(outBuffer, maxLength, scriptPath.c_str(), _TRUNCATE);
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeScript_Set(uint64_t entityID, const char* inTag)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        entity.GetComponent<ScriptComponent>().ClassName = inTag;
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeRigidbody_Get(uint64_t entityID, RigidbodyComponent* outData)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        *outData = entity.GetComponent<RigidbodyComponent>();
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeRigidbody_Set(uint64_t entityID, RigidbodyComponent* inData)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        auto& rb = entity.GetComponent<RigidbodyComponent>();

        void* tempActor = rb.RuntimeActor;
        rb = *inData;
        rb.RuntimeActor = tempActor;

        if (rb.RuntimeActor && rb.Type != RigidbodyComponent::BodyType::Static)
        {
            physx::PxRigidDynamic* actor = (physx::PxRigidDynamic*)rb.RuntimeActor;
            actor->setMass(rb.Mass);
            actor->setLinearDamping(rb.LinearDrag);
            actor->setAngularDamping(rb.AngularDrag);
        }
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeBoxCollider_Get(uint64_t entityID, BoxColliderComponent* outData)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        *outData = entity.GetComponent<BoxColliderComponent>();
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeBoxCollider_Set(uint64_t entityID, BoxColliderComponent* inData)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        auto& bc = entity.GetComponent<BoxColliderComponent>();

        void* tempShape = bc.RuntimeShape;
        void* tempMat = bc.RuntimeMaterial;

        bc = *inData;

        bc.RuntimeShape = tempShape;
        bc.RuntimeMaterial = tempMat;
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeSphereCollider_Get(uint64_t entityID, SphereColliderComponent* outData)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        *outData = entity.GetComponent<SphereColliderComponent>();
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeSphereCollider_Set(uint64_t entityID, SphereColliderComponent* inData)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        auto& bc = entity.GetComponent<SphereColliderComponent>();

        void* tempShape = bc.RuntimeShape;
        void* tempMat = bc.RuntimeMaterial;

        bc = *inData;

        bc.RuntimeShape = tempShape;
        bc.RuntimeMaterial = tempMat;
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeCapsuleCollider_Get(uint64_t entityID, CapsuleColliderComponent* outData)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        *outData = entity.GetComponent<CapsuleColliderComponent>();
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeCapsuleCollider_Set(uint64_t entityID, CapsuleColliderComponent* inData)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        auto& bc = entity.GetComponent<CapsuleColliderComponent>();

        void* tempShape = bc.RuntimeShape;
        void* tempMat = bc.RuntimeMaterial;

        bc = *inData;

        bc.RuntimeShape = tempShape;
        bc.RuntimeMaterial = tempMat;
    }
#pragma endregion

    void ScriptInterop::RegisterFunctions(InternalCalls* outCalls)
    {
        RXN_CORE_ASSERT(outCalls, "InternalCalls struct is null!");

        //Logging & Core
        outCalls->LogMessage = (void*)NativeLogMessage;
        outCalls->ScriptField_Register = (void*)NativeScriptField_Register;

        //Input
        outCalls->Input_IsKeyDown = (void*)NativeInput_IsKeyDown;
		outCalls->NativeInput_GetMousePosition = (void*)NativeInput_GetMousePosition;
		outCalls->NativeInput_SetCursorMode = (void*)NativeInput_SetCursorMode;

        //Entity Lifecycle
        outCalls->Entity_Create = (void*)NativeEntity_Create;
        outCalls->Entity_Destroy = (void*)NativeEntity_Destroy;
		outCalls->NativeEntity_InstantiatePrefab = (void*)NativeEntity_InstantiatePrefab;
		outCalls->NativeEntity_FindByName = (void*)NativeEntity_FindByName;

        //Transform Math
        outCalls->NativeEntity_GetWorldPosition = (void*)NativeEntity_GetWorldPosition;
        outCalls->Entity_GetTranslation = (void*)NativeEntity_GetTranslation;
        outCalls->Entity_SetTranslation = (void*)NativeEntity_SetTranslation;
		outCalls->NativeEntity_GetRotation = (void*)NativeEntity_GetRotation;
		outCalls->NativeEntity_SetRotation = (void*)NativeEntity_SetRotation;
		outCalls->NativeEntity_GetForward = (void*)NativeEntity_GetForward;
		outCalls->NativeEntity_GetRight = (void*)NativeEntity_GetRight;
		outCalls->NativeEntity_GetUp = (void*)NativeEntity_GetUp;

        //Physics Interop
		outCalls->NativeRigidbody_ApplyLinearImpulse = (void*)NativeRigidbody_ApplyLinearImpulse;
        outCalls->Physics_Raycast = (void*)NativePhysics_Raycast;

        //Component Accessors
        outCalls->NativeEntity_HasComponent = (void*)NativeEntity_HasComponent;

        outCalls->NativeTag_Get = (void*)NativeTag_Get;
        outCalls->NativeTag_Set = (void*)NativeTag_Set;

        outCalls->NativeTransform_Get = (void*)NativeTransform_Get;
        outCalls->NativeTransform_Set = (void*)NativeTransform_Set;

        outCalls->NativeRelationship_GetParent = (void*)NativeRelationship_GetParent;
        outCalls->NativeRelationship_SetParent = (void*)NativeRelationship_SetParent;
         
        outCalls->NativeStaticMesh_GetAssetPath = (void*)NativeStaticMesh_GetAssetPath;
        outCalls->NativeStaticMesh_SetAssetPath = (void*)NativeStaticMesh_SetAssetPath;
         
        //CameraComponent

        outCalls->NativeDirLight_Get = (void*)NativeDirLight_Get;
        outCalls->NativeDirLight_Set = (void*)NativeDirLight_Set;

        outCalls->NativePointLight_Get = (void*)NativePointLight_Get;
        outCalls->NativePointLight_Set = (void*)NativePointLight_Set;

        outCalls->NativeScript_Get = (void*)NativeScript_Get;
        outCalls->NativeScript_Set = (void*)NativeScript_Set;

        outCalls->NativeRigidbody_Get = (void*)NativeRigidbody_Get;
        outCalls->NativeRigidbody_Set = (void*)NativeRigidbody_Set;

        outCalls->NativeBoxCollider_Get = (void*)NativeBoxCollider_Get;
        outCalls->NativeBoxCollider_Set = (void*)NativeBoxCollider_Set;

        outCalls->NativeSphereCollider_Get = (void*)NativeSphereCollider_Get;
        outCalls->NativeSphereCollider_Set = (void*)NativeSphereCollider_Set;

        outCalls->NativeCapsuleCollider_Get = (void*)NativeCapsuleCollider_Get;
        outCalls->NativeCapsuleCollider_Set = (void*)NativeCapsuleCollider_Set;

    }

}