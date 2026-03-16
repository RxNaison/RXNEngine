using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.IO;

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

        private static Assembly? s_AppAssembly = null;

        private static Dictionary<ulong, object> s_EntityInstances = new();

        [UnmanagedCallersOnly]
        public static void LoadGameScripts(IntPtr corePathPtr, IntPtr appPathPtr)
        {
            string? corePath = Marshal.PtrToStringUTF8(corePathPtr);
            string? appPath = Marshal.PtrToStringUTF8(appPathPtr);
            if (corePath == null || appPath == null) return;

            s_ALC = new GameScriptALC();

            try
            {
                string corePdb = Path.ChangeExtension(corePath, ".pdb");
                using (var coreStream = new FileStream(corePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                {
                    if (File.Exists(corePdb))
                    {
                        using var corePdbStream = new FileStream(corePdb, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
                        s_CoreAssembly = s_ALC.LoadFromStream(coreStream, corePdbStream);
                    }
                    else s_CoreAssembly = s_ALC.LoadFromStream(coreStream);
                }

                string appPdb = Path.ChangeExtension(appPath, ".pdb");
                using (var appStream = new FileStream(appPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                {
                    if (File.Exists(appPdb))
                    {
                        using var appPdbStream = new FileStream(appPdb, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
                        s_AppAssembly = s_ALC.LoadFromStream(appStream, appPdbStream);
                    }
                    else s_AppAssembly = s_ALC.LoadFromStream(appStream);
                }

                Console.WriteLine($"[.NET Host] Successfully loaded Game Scripts.");
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
        public static void InstantiateScript(ulong entityID, IntPtr classNamePtr)
        {
            if (s_AppAssembly == null) return;

            string? className = Marshal.PtrToStringUTF8(classNamePtr);
            if (className == null) return;

            Type? scriptType = s_AppAssembly.GetType(className);
            if (scriptType != null)
            {
                object? instance = Activator.CreateInstance(scriptType);
                if (instance != null)
                {
                    var idProperty = scriptType.BaseType?.GetProperty("ID", BindingFlags.Public | BindingFlags.Instance);
                    idProperty?.SetValue(instance, entityID);

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
            if (s_AppAssembly == null) return 0;

            string? className = Marshal.PtrToStringUTF8(classNamePtr);
            if (className == null) return 0;

            Type? scriptType = s_AppAssembly.GetType(className);

            return scriptType != null ? 1 : 0;
        }

        [UnmanagedCallersOnly]
        public static void ReflectClass(IntPtr classNamePtr)
        {
            string? className = Marshal.PtrToStringUTF8(classNamePtr);
            if (className == null || s_AppAssembly == null) return;

            Type? type = s_AppAssembly.GetType(className);
            if (type == null) return;

            IntPtr classStr = Marshal.StringToHGlobalAnsi(className);

            foreach (var field in type.GetFields(BindingFlags.Public | BindingFlags.Instance))
            {
                uint fieldType = 0;
                if (field.FieldType == typeof(float)) fieldType = 1;

                if (fieldType != 0)
                {
                    IntPtr fieldStr = Marshal.StringToHGlobalAnsi(field.Name);

                    unsafe
                    {
                        var registerFunc = (delegate* unmanaged<IntPtr, IntPtr, uint, void>)Interop.NativeFunctions.ScriptField_Register;
                        registerFunc(classStr, fieldStr, fieldType);
                    }

                    Marshal.FreeHGlobal(fieldStr);
                }
            }
            Marshal.FreeHGlobal(classStr);
        }

        [UnmanagedCallersOnly]
        public static float GetFloatField(ulong entityID, IntPtr fieldNamePtr)
        {
            string? fieldName = Marshal.PtrToStringUTF8(fieldNamePtr);
            if (fieldName != null && s_EntityInstances.TryGetValue(entityID, out object? instance))
            {
                var field = instance.GetType().GetField(fieldName);
                if (field != null)
                    return (float)field.GetValue(instance)!;
            }
            return 0.0f;
        }

        [UnmanagedCallersOnly]
        public static void SetFloatField(ulong entityID, IntPtr fieldNamePtr, float value)
        {
            string? fieldName = Marshal.PtrToStringUTF8(fieldNamePtr);
            if (fieldName != null && s_EntityInstances.TryGetValue(entityID, out object? instance))
            {
                var field = instance.GetType().GetField(fieldName);
                field?.SetValue(instance, value);
            }
        }
    }
}