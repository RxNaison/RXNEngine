using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

namespace RXNScriptHost
{
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

                Type? mainType = s_CoreAssembly.GetType("RXNEngine.Player");
                if (mainType != null)
                {
                    //Activator.CreateInstance(mainType);
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

        private static Dictionary<ulong, object> s_EntityInstances = new();

        [UnmanagedCallersOnly]
        public static void InstantiateScript(ulong entityID, IntPtr classNamePtr)
        {
            if (s_CoreAssembly == null) return;

            string? className = Marshal.PtrToStringUTF8(classNamePtr);
            if (className == null) return;

            Type? scriptType = s_CoreAssembly.GetType(className);
            if (scriptType != null)
            {
                object? instance = Activator.CreateInstance(scriptType);
                if (instance != null)
                {
                    var idField = scriptType.BaseType?.GetField("ID", BindingFlags.Public | BindingFlags.Instance);
                    idField?.SetValue(instance, entityID);

                    s_EntityInstances[entityID] = instance;
                }
            }
            else
            {
                Console.WriteLine($"[.NET Host] Could not find script class: {className}");
            }
        }

        [UnmanagedCallersOnly]
        public static void InvokeOnCreate(ulong entityID)
        {
            if (s_EntityInstances.TryGetValue(entityID, out object? instance))
            {
                MethodInfo? onCreate = instance.GetType().GetMethod("OnCreate");
                onCreate?.Invoke(instance, null);
            }
        }

        [UnmanagedCallersOnly]
        public static void InvokeOnUpdate(ulong entityID, float deltaTime)
        {
            if (s_EntityInstances.TryGetValue(entityID, out object? instance))
            {
                MethodInfo? onUpdate = instance.GetType().GetMethod("OnUpdate");
                onUpdate?.Invoke(instance, new object[] { deltaTime });
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

        [UnmanagedCallersOnly]
        public static int EntityClassExists(IntPtr classNamePtr)
        {
            if (s_CoreAssembly == null) return 0;

            string? className = Marshal.PtrToStringUTF8(classNamePtr);
            if (className == null) return 0;

            Type? scriptType = s_CoreAssembly.GetType(className);

            return scriptType != null ? 1 : 0;
        }
    }
}