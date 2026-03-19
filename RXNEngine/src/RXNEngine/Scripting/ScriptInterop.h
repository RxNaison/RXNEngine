#pragma once

#include <glm/glm.hpp>

namespace RXNEngine {

    struct RaycastHit
    {
        uint64_t EntityID;
        glm::vec3 Position;
        glm::vec3 Normal;
        float Distance;
    };

    struct InternalCalls
    {
        //Logging & Core
        void* LogMessage = nullptr;
        void* ScriptField_Register = nullptr;

        //Input
        void* Input_IsKeyDown = nullptr;
        void* NativeInput_GetMousePosition = nullptr;
        void* NativeInput_SetCursorMode = nullptr;

        //Entity Lifecycle
        void* Entity_Create = nullptr;
        void* Entity_Destroy = nullptr;
        void* NativeEntity_InstantiatePrefab = nullptr;
        void* NativeEntity_FindByName = nullptr;

        //Transform Math
        void* NativeEntity_GetWorldPosition = nullptr;
        void* Entity_GetTranslation = nullptr;
        void* Entity_SetTranslation = nullptr;
        void* NativeEntity_GetRotation = nullptr;
        void* NativeEntity_SetRotation = nullptr;
        void* NativeEntity_GetForward = nullptr;
        void* NativeEntity_GetRight = nullptr;
        void* NativeEntity_GetUp = nullptr;

        //Physics Interop
        void* NativeRigidbody_ApplyLinearImpulse = nullptr;
        void* Physics_Raycast = nullptr;
    };

    class ScriptInterop
    {
    public:
        static void RegisterFunctions(InternalCalls* outCalls);
    };

}