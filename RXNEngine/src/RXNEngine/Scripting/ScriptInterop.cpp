#include "rxnpch.h"
#include "ScriptInterop.h"
#include "ScriptEngine.h"
#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Core/Input.h"
#include "RXNEngine/Core/KeyCodes.h"

#include <coreclr_delegates.h>
#include <PxPhysicsAPI.h>

namespace RXNEngine {

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeLogMessage(const char* message)
    {
        RXN_CORE_WARN("C# SAYS: {0}", message);
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

        if(entity.HasComponent<RigidbodyComponent>())
            scene->SyncTransformToPhysics(entity);
    }

    extern "C" uint8_t CORECLR_DELEGATE_CALLTYPE NativeInput_IsKeyDown(uint32_t keycode)
    {
        return Input::IsKeyPressed((KeyCode)keycode) ? 1 : 0;
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeScriptField_Register(const char* className, const char* fieldName, uint32_t type)
    {
        ScriptEngine::RegisterField(className, fieldName, (ScriptFieldType)type);
    }

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

    void ScriptInterop::RegisterFunctions(InternalCalls* outCalls)
    {
        RXN_CORE_ASSERT(outCalls, "InternalCalls struct is null!");

        outCalls->LogMessage = (void*)NativeLogMessage;
        outCalls->Entity_GetTranslation = (void*)NativeEntity_GetTranslation;
        outCalls->Entity_SetTranslation = (void*)NativeEntity_SetTranslation;
        outCalls->Input_IsKeyDown = (void*)NativeInput_IsKeyDown;
        outCalls->ScriptField_Register = (void*)NativeScriptField_Register;
        outCalls->Entity_Create = (void*)NativeEntity_Create;
        outCalls->Entity_Destroy = (void*)NativeEntity_Destroy;
		outCalls->NativeEntity_FindByName = (void*)NativeEntity_FindByName;
		outCalls->NativeEntity_InstantiatePrefab = (void*)NativeEntity_InstantiatePrefab;
		outCalls->NativeRigidbody_ApplyLinearImpulse = (void*)NativeRigidbody_ApplyLinearImpulse;
    }

}