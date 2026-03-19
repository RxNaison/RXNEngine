#include "rxnpch.h"
#include "ScriptInterop.h"
#include "ScriptEngine.h"
#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Core/Input.h"
#include "RXNEngine/Core/KeyCodes.h"
#include "RXNEngine/Core/Application.h"

#include <coreclr_delegates.h>
#include <PxPhysicsAPI.h>

namespace RXNEngine {

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
        outCalls->Entity_GetTranslation = (void*)NativeEntity_GetTranslation;
        outCalls->Entity_SetTranslation = (void*)NativeEntity_SetTranslation;
		outCalls->NativeEntity_GetRotation = (void*)NativeEntity_GetRotation;
		outCalls->NativeEntity_SetRotation = (void*)NativeEntity_SetRotation;
		outCalls->NativeEntity_GetForward = (void*)NativeEntity_GetForward;
		outCalls->NativeEntity_GetRight = (void*)NativeEntity_GetRight;
		outCalls->NativeEntity_GetUp = (void*)NativeEntity_GetUp;

        //Physics Interop
		outCalls->NativeRigidbody_ApplyLinearImpulse = (void*)NativeRigidbody_ApplyLinearImpulse;
    }

}