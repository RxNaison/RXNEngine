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
        void* NativeAssetManager_LoadMeshAsync = nullptr;

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
        void* NativeEntity_GetForward = nullptr;
        void* NativeEntity_GetRight = nullptr;
        void* NativeEntity_GetUp = nullptr;

        //Physics Interop
        void* NativeRigidbody_ApplyLinearImpulse = nullptr;
        void* Physics_Raycast = nullptr;

        //Component Accessors
        void* NativeEntity_HasComponent = nullptr;
        void* NativeEntity_AddComponent = nullptr;

        void* NativeTag_Get = nullptr;
        void* NativeTag_Set = nullptr;

        void* NativeTransform_Get = nullptr;
        void* NativeTransform_Set = nullptr;

        void* NativeRelationship_GetParent = nullptr;
        void* NativeRelationship_SetParent = nullptr;

        void* NativeStaticMesh_GetAssetPath = nullptr;
        void* NativeStaticMesh_SetAssetPath = nullptr;

        //CameraComponent

        void* NativeDirLight_Get = nullptr;
        void* NativeDirLight_Set = nullptr;

        void* NativePointLight_Get = nullptr;
        void* NativePointLight_Set = nullptr;

        void* NativeScript_Get = nullptr;
        void* NativeScript_Set = nullptr;

        void* NativeRigidbody_Get = nullptr;
        void* NativeRigidbody_Set = nullptr;

        void* NativeBoxCollider_Get = nullptr;
        void* NativeBoxCollider_Set = nullptr;

        void* NativeSphereCollider_Get = nullptr;
        void* NativeSphereCollider_Set = nullptr;

        void* NativeCapsuleCollider_Get = nullptr;
        void* NativeCapsuleCollider_Set = nullptr;
    };

    class ScriptInterop
    {
    public:
        static void RegisterFunctions(InternalCalls* outCalls);
    };

}