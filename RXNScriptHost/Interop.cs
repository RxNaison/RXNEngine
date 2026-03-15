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
    }

    public static class Interop
    {
        public static InternalCalls NativeFunctions;

        [UnmanagedCallersOnly]
        public static void RegisterInternalCalls(IntPtr internalCallsPtr)
        {
            NativeFunctions = Marshal.PtrToStructure<InternalCalls>(internalCallsPtr);
            Console.WriteLine("[.NET Host] Successfully registered C++ ScriptInterop.");
        }
    }
}