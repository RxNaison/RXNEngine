#pragma once

namespace RXNEngine {

    struct InternalCalls
    {
        void* LogMessage = nullptr;
        void* Entity_GetTranslation = nullptr;
        void* Entity_SetTranslation = nullptr;
        void* Input_IsKeyDown = nullptr;
        void* ScriptField_Register = nullptr;
        void* Entity_Create = nullptr;
        void* Entity_Destroy = nullptr;
        void* NativeEntity_FindByName = nullptr;
        void* NativeEntity_InstantiatePrefab = nullptr;
        void* NativeRigidbody_ApplyLinearImpulse = nullptr;
        void* NativeEntity_GetForward = nullptr;
        void* NativeEntity_GetRight = nullptr;
        void* NativeEntity_GetUp = nullptr;
        void* NativeInput_GetMousePosition = nullptr;
        void* NativeInput_SetCursorMode = nullptr;
        void* NativeEntity_GetRotation = nullptr;
        void* NativeEntity_SetRotation = nullptr;
    };

    class ScriptInterop
    {
    public:
        static void RegisterFunctions(InternalCalls* outCalls);
    };

}