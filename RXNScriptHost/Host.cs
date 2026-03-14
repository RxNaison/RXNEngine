using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

namespace RXNScriptHost
{
    [StructLayout(LayoutKind.Sequential)]
    public struct InternalCalls
    {
        public IntPtr LogMessage;
    }

    class GameScriptALC : AssemblyLoadContext
    {
        public GameScriptALC() : base("GameScriptALC", isCollectible: true) { }

        protected override Assembly? Load(AssemblyName assemblyName)
        {
            if (assemblyName.Name == "RXNScriptHost")
            {
                return typeof(Host).Assembly;
            }

            return null;
        }
    }

    public static class Host
    {
        private static GameScriptALC? s_ALC = null;
        private static Assembly? s_CoreAssembly = null;

        public static InternalCalls NativeFunctions;

        [UnmanagedCallersOnly]
        public static void RegisterInternalCalls(IntPtr internalCallsPtr)
        {
            NativeFunctions = Marshal.PtrToStructure<InternalCalls>(internalCallsPtr);
            Console.WriteLine("[.NET Host] Successfully registered C++ Internal Calls.");
        }

        [UnmanagedCallersOnly]
        public static void LoadGameScripts(IntPtr assemblyPathPtr)
        {
            string? assemblyPath = Marshal.PtrToStringUTF8(assemblyPathPtr);
            if (assemblyPath == null) return;

            s_ALC = new GameScriptALC();

            try
            {
                s_CoreAssembly = s_ALC.LoadFromAssemblyPath(assemblyPath);
                Console.WriteLine($"[.NET Host] Successfully loaded Game Scripts from: {assemblyPath}");

                Type? mainType = s_CoreAssembly.GetType("RXNEngine.Main");
                if (mainType != null)
                {
                    Activator.CreateInstance(mainType);
                }
                else
                {
                    Console.WriteLine("[.NET Host] Warning: Could not find class RXNEngine.Main");
                }
            }
            catch (Exception e)
            {
                Console.WriteLine($"[.NET Host] Failed to load Game Scripts: {e.Message}");
                if (e.InnerException != null)
                {
                    Console.WriteLine($"[.NET Host] CRASH REASON: {e.InnerException.Message}");
                }
            }
        }

        [UnmanagedCallersOnly]
        public static void UnloadGameScripts()
        {
            if (s_ALC == null) return;

            s_ALC.Unload();
            s_ALC = null;
            s_CoreAssembly = null;

            GC.Collect();
            GC.WaitForPendingFinalizers();

            Console.WriteLine("[.NET Host] Unloaded Game Scripts. Ready for Hot-Reload!");
        }
    }
}