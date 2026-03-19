using System;
using System.Runtime.InteropServices;

namespace RXNScriptHost
{
    [StructLayout(LayoutKind.Sequential)]
    public struct InternalCalls
    {
        //Logging & Core
        public IntPtr LogMessage;
        public IntPtr ScriptField_Register;

        //Input
        public IntPtr Input_IsKeyDown;
        public IntPtr Input_GetMousePosition;
        public IntPtr Input_SetCursorMode;

        //Entity Lifecycle
        public IntPtr Entity_Create;
        public IntPtr Entity_Destroy;
        public IntPtr Entity_InstantiatePrefab;
        public IntPtr Entity_FindByName;

        //Transform Math
        public IntPtr Entity_GetWorldPosition;
        public IntPtr Entity_GetTranslation;
        public IntPtr Entity_SetTranslation;
        public IntPtr Entity_GetRotation;
        public IntPtr Entity_SetRotation;
        public IntPtr Entity_GetForward;
        public IntPtr Entity_GetRight;
        public IntPtr Entity_GetUp;

        //Physics Interop
        public IntPtr Rigidbody_ApplyLinearImpulse;
        public IntPtr Physics_Raycast;
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