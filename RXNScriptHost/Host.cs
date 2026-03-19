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

        private static uint GetScriptFieldType(Type type)
        {
            if (type == typeof(float)) return 1;
            if (type == typeof(double)) return 2;
            if (type == typeof(bool)) return 3;
            if (type == typeof(char)) return 4;
            if (type == typeof(sbyte)) return 5;
            if (type == typeof(short)) return 6;
            if (type == typeof(int)) return 7;
            if (type == typeof(long)) return 8;
            if (type == typeof(byte)) return 9;
            if (type == typeof(ushort)) return 10;
            if (type == typeof(uint)) return 11;
            if (type == typeof(ulong)) return 12;

            if (type.Name == "Vector2") return 13;
            if (type.Name == "Vector3") return 14;
            if (type.Name == "Vector4") return 15;

            Type? entityBaseType = s_CoreAssembly?.GetType("RXNEngine.Entity");
            if (entityBaseType != null && entityBaseType.IsAssignableFrom(type)) return 16;

            return 0;
        }

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
        public static void InvokeOnDestroy(ulong entityID)
        {
            if (s_EntityInstances.TryGetValue(entityID, out object? instance))
            {
                var method = instance.GetType().GetMethod("OnDestroy", BindingFlags.Public | BindingFlags.Instance);

                if (method != null && method.DeclaringType != s_CoreAssembly?.GetType("RXNEngine.Entity"))
                {
                    method.Invoke(instance, null);
                }

                s_EntityInstances.Remove(entityID);
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
        public static void InvokeOnFixedUpdate(ulong entityID, float fixedTimeStep)
        {
            if (s_EntityInstances.TryGetValue(entityID, out object? instance))
            {
                var method = instance.GetType().GetMethod("OnFixedUpdate", BindingFlags.Public | BindingFlags.Instance);

                if (method != null && method.DeclaringType != s_CoreAssembly?.GetType("RXNEngine.Entity"))
                {
                    method.Invoke(instance, new object[] { fixedTimeStep });
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
                uint fieldType = GetScriptFieldType(field.FieldType);

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
        public static void GetFieldValue(ulong entityID, IntPtr fieldNamePtr, IntPtr outBuffer)
        {
            string? fieldName = Marshal.PtrToStringUTF8(fieldNamePtr);
            if (fieldName != null && s_EntityInstances.TryGetValue(entityID, out object? instance))
            {
                var field = instance.GetType().GetField(fieldName);
                if (field != null)
                {
                    object? value = field.GetValue(instance);
                    if (value == null) return;

                    if (field.FieldType == s_CoreAssembly?.GetType("RXNEngine.Entity"))
                    {
                        var idProp = value.GetType().GetProperty("ID", BindingFlags.Public | BindingFlags.Instance);
                        ulong targetID = idProp != null ? (ulong)idProp.GetValue(value)! : 0;
                        Marshal.WriteInt64(outBuffer, (long)targetID);
                    }
                    else
                    {
                        Marshal.StructureToPtr(value, outBuffer, false);
                    }
                }
            }
        }

        [UnmanagedCallersOnly]
        public static void SetFieldValue(ulong entityID, IntPtr fieldNamePtr, IntPtr inBuffer)
        {
            string? fieldName = Marshal.PtrToStringUTF8(fieldNamePtr);
            if (fieldName != null && s_EntityInstances.TryGetValue(entityID, out object? instance))
            {
                var field = instance.GetType().GetField(fieldName);
                if (field != null)
                {
                    if (field.FieldType == s_CoreAssembly?.GetType("RXNEngine.Entity"))
                    {
                        ulong targetID = (ulong)Marshal.ReadInt64(inBuffer);
                        if (targetID == 0) field.SetValue(instance, null);
                        else
                        {
                            object? newEntity = Activator.CreateInstance(field.FieldType, true);
                            var idProp = field.FieldType.GetProperty("ID", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);
                            idProp?.SetValue(newEntity, targetID);
                            field.SetValue(instance, newEntity);
                        }
                    }
                    else
                    {
                        object value = Marshal.PtrToStructure(inBuffer, field.FieldType)!;
                        field.SetValue(instance, value);
                    }
                }
            }
        }

        [UnmanagedCallersOnly]
        public static void OnCollisionEnter(ulong entityID, ulong otherEntityID)
        {
            if (s_EntityInstances.TryGetValue(entityID, out object? instance))
            {
                var method = instance.GetType().GetMethod("OnCollisionEnter", BindingFlags.Public | BindingFlags.Instance);

                if (method != null && method.DeclaringType != s_CoreAssembly?.GetType("RXNEngine.Entity"))
                {
                    Type? entityType = s_CoreAssembly?.GetType("RXNEngine.Entity");
                    object? otherEntity = Activator.CreateInstance(entityType!, true);
                    entityType!.GetProperty("ID")?.SetValue(otherEntity, otherEntityID);

                    method.Invoke(instance, new object[] { otherEntity });
                }
            }
        }

        [UnmanagedCallersOnly]
        public static void OnCollisionExit(ulong entityID, ulong otherEntityID)
        {
            if (s_EntityInstances.TryGetValue(entityID, out object? instance))
            {
                var method = instance.GetType().GetMethod("OnCollisionExit", BindingFlags.Public | BindingFlags.Instance);

                if (method != null && method.DeclaringType != s_CoreAssembly?.GetType("RXNEngine.Entity"))
                {
                    Type? entityType = s_CoreAssembly?.GetType("RXNEngine.Entity");
                    object? otherEntity = Activator.CreateInstance(entityType!, true);
                    entityType!.GetProperty("ID")?.SetValue(otherEntity, otherEntityID);

                    method.Invoke(instance, new object[] { otherEntity });
                }
            }
        }

        [UnmanagedCallersOnly]
        public static void OnTriggerEnter(ulong entityID, ulong otherEntityID)
        {
            if (s_EntityInstances.TryGetValue(entityID, out object? instance))
            {
                var method = instance.GetType().GetMethod("OnTriggerEnter", BindingFlags.Public | BindingFlags.Instance);

                if (method != null && method.DeclaringType != s_CoreAssembly?.GetType("RXNEngine.Entity"))
                {
                    Type? entityType = s_CoreAssembly?.GetType("RXNEngine.Entity");
                    object? otherEntity = Activator.CreateInstance(entityType!, true);
                    entityType!.GetProperty("ID")?.SetValue(otherEntity, otherEntityID);

                    method.Invoke(instance, new object[] { otherEntity });
                }
            }
        }

        [UnmanagedCallersOnly]
        public static void OnTriggerExit(ulong entityID, ulong otherEntityID)
        {
            if (s_EntityInstances.TryGetValue(entityID, out object? instance))
            {
                var method = instance.GetType().GetMethod("OnTriggerExit", BindingFlags.Public | BindingFlags.Instance);

                if (method != null && method.DeclaringType != s_CoreAssembly?.GetType("RXNEngine.Entity"))
                {
                    Type? entityType = s_CoreAssembly?.GetType("RXNEngine.Entity");
                    object? otherEntity = Activator.CreateInstance(entityType!, true);
                    entityType!.GetProperty("ID")?.SetValue(otherEntity, otherEntityID);

                    method.Invoke(instance, new object[] { otherEntity });
                }
            }
        }
    }
}