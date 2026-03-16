#include "rxnpch.h"
#include "ScriptInterop.h"
#include "ScriptEngine.h"
#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Core/Input.h"
#include "RXNEngine/Core/KeyCodes.h"

#include <coreclr_delegates.h>

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
        *outTranslation = entity.GetComponent<TransformComponent>().Translation;
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeEntity_SetTranslation(uint64_t entityID, glm::vec3* inTranslation)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        RXN_CORE_ASSERT(scene, "No active scene!");
        Entity entity = scene->GetEntityByUUID(entityID);
        entity.GetComponent<TransformComponent>().Translation = *inTranslation;
    }

    extern "C" uint8_t CORECLR_DELEGATE_CALLTYPE NativeInput_IsKeyDown(uint32_t keycode)
    {
        return Input::IsKeyPressed((KeyCode)keycode) ? 1 : 0;
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeScriptField_Register(const char* className, const char* fieldName, uint32_t type)
    {
        ScriptEngine::RegisterField(className, fieldName, (ScriptFieldType)type);
    }

    void ScriptInterop::RegisterFunctions(InternalCalls* outCalls)
    {
        RXN_CORE_ASSERT(outCalls, "InternalCalls struct is null!");

        outCalls->LogMessage = (void*)NativeLogMessage;
        outCalls->Entity_GetTranslation = (void*)NativeEntity_GetTranslation;
        outCalls->Entity_SetTranslation = (void*)NativeEntity_SetTranslation;
        outCalls->Input_IsKeyDown = (void*)NativeInput_IsKeyDown;
        outCalls->ScriptField_Register = (void*)NativeScriptField_Register;
    }

}