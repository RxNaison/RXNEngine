#pragma once

namespace RXNEngine {

    struct InternalCalls
    {
        void* LogMessage = nullptr;
        void* Entity_GetTranslation = nullptr;
        void* Entity_SetTranslation = nullptr;
        void* Input_IsKeyDown = nullptr;
        void* ScriptField_Register = nullptr;
    };

    class ScriptInterop
    {
    public:
        static void RegisterFunctions(InternalCalls* outCalls);
    };

}