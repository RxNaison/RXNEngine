using System;
using System.Runtime.InteropServices;

namespace RXNScriptHost
{
    [StructLayout(LayoutKind.Sequential)]
    public struct InternalCalls
    {
        public IntPtr LogMessage;
        public IntPtr Entity_GetTranslation;
        public IntPtr Entity_SetTranslation;
        public IntPtr Input_IsKeyDown;
        public IntPtr ScriptField_Register;
        public IntPtr Entity_Create;
        public IntPtr Entity_Destroy;
        public IntPtr Entity_FindByName;
        public IntPtr Entity_InstantiatePrefab;
        public IntPtr Rigidbody_ApplyLinearImpulse;
        public IntPtr Entity_GetForward;
        public IntPtr Entity_GetRight;
        public IntPtr Entity_GetUp;
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