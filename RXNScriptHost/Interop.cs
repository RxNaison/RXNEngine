using System;
using System.Numerics;
using System.Runtime.InteropServices;

namespace RXNScriptHost
{
    [StructLayout(LayoutKind.Sequential)]
    public struct InternalCalls
    {
        //Logging & Core
        public IntPtr LogMessage;
        public IntPtr ScriptField_Register;
        public IntPtr AssetManager_LoadMeshAsync;

        //Input
        public IntPtr Input_IsKeyDown;
        public IntPtr Input_GetMousePosition;
        public IntPtr Input_GetMouseDelta;
        public IntPtr Input_SetCursorMode;

        public IntPtr Input_IsGamepadButtonDown;
        public IntPtr Input_GetGamepadAxis;
        public IntPtr Input_GetGamepadDeadzone;
        public IntPtr Input_SetGamepadDeadzone;
        public IntPtr Input_SetGamepadVibration;

        //Entity Lifecycle
        public IntPtr Entity_Create;
        public IntPtr Entity_Destroy;
        public IntPtr Entity_InstantiatePrefab;
        public IntPtr Entity_FindByName;

        //Transform Math
        public IntPtr Entity_GetWorldPosition;
        public IntPtr Entity_GetForward;
        public IntPtr Entity_GetRight;
        public IntPtr Entity_GetUp;

        //Physics Interop
        public IntPtr Rigidbody_ApplyLinearImpulse;
        public IntPtr Physics_Raycast;

        //Component Accessors
        public IntPtr Entity_HasComponent;
        public IntPtr Entity_AddComponent;

        public IntPtr Entity_Tag_Get;
        public IntPtr Entity_Tag_Set;

        public IntPtr Entity_Transform_Get;
        public IntPtr Entity_Transform_Set;

        public IntPtr Entity_Relationship_GetParent;
        public IntPtr Entity_Relationship_SetParent;

        public IntPtr Entity_StaticMesh_GetAssetPath;
        public IntPtr Entity_StaticMesh_SetAssetPath;

        //CameraComponent
        public IntPtr DirLight_Get;
        public IntPtr DirLight_Set;

        public IntPtr PointLight_Get;
        public IntPtr PointLight_Set;

        public IntPtr Entity_Script_Get;
        public IntPtr Entity_Script_Set;

        public IntPtr Entity_Rigidbody_Get;
        public IntPtr Entity_Rigidbody_Set;

        public IntPtr Entity_BoxCollider_Get;
        public IntPtr Entity_BoxCollider_Set;

        public IntPtr Entity_SphereCollider_Get;
        public IntPtr Entity_SphereCollider_Set;

        public IntPtr Entity_CapsuleCollider_Get;
        public IntPtr Entity_CapsuleCollider_Set;

        public IntPtr CharacterController_Move;
    }

    public static class Interop
    {
        public static InternalCalls NativeFunctions;

        [UnmanagedCallersOnly]
        public static void RegisterInternalCalls(IntPtr internalCallsPtr)
        {
            NativeFunctions = Marshal.PtrToStructure<InternalCalls>(internalCallsPtr);
        }
    }
}