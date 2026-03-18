using System;
using System.Runtime.InteropServices;
using RXNScriptHost;

namespace RXNEngine
{
    public class Entity
    {
        public ulong ID { get; internal set; }
        protected Entity() { ID = 0; }

        internal Entity(ulong id)
        {
            ID = id;
        }

        public static Entity Instantiate()
        {
            unsafe
            {
                var create = (delegate* unmanaged<ulong>)Interop.NativeFunctions.Entity_Create;
                ulong newID = create();
                return new Entity() { ID = newID };
            }
        }
        public static Entity Instantiate(Entity original)
        {
            unsafe
            {
                var instantiate = (delegate* unmanaged<ulong, ulong>)Interop.NativeFunctions.Entity_InstantiatePrefab;
                ulong newID = instantiate(original.ID);
                return new Entity() { ID = newID };
            }
        }

        public void Destroy()
        {
            unsafe
            {
                var destroy = (delegate* unmanaged<ulong, void>)Interop.NativeFunctions.Entity_Destroy;
                destroy(ID);
            }
        }

        public virtual void OnCreate() { }
        public virtual void OnDestroy() { }
        public virtual void OnUpdate(float deltaTime) { }
        public virtual void OnFixedUpdate(float deltaTime) { }

        public virtual void OnCollisionEnter(Entity other) { }
        public virtual void OnCollisionExit(Entity other) { }

        public Vector3 Translation
        {
            get
            {
                Vector3 result;
                unsafe
                {
                    var getTranslation = (delegate* unmanaged<ulong, Vector3*, void>)Interop.NativeFunctions.Entity_GetTranslation;
                    getTranslation(ID, &result);
                }
                return result;
            }
            set
            {
                unsafe
                {
                    var setTranslation = (delegate* unmanaged<ulong, Vector3*, void>)Interop.NativeFunctions.Entity_SetTranslation;
                    setTranslation(ID, &value);
                }
            }
        }

        public Vector3 Rotation
        {
            get
            {
                unsafe
                {
                    var func = (delegate* unmanaged<ulong, Vector3*, void>)Interop.NativeFunctions.Entity_GetRotation;
                    Vector3 result;
                    func(ID, &result);
                    return result;
                }
            }
            set
            {
                unsafe
                {
                    var func = (delegate* unmanaged<ulong, Vector3*, void>)Interop.NativeFunctions.Entity_SetRotation;
                    func(ID, &value);
                }
            }
        }

        public static Entity? FindEntityByName(string name)
        {
            unsafe
            {
                IntPtr namePtr = Marshal.StringToHGlobalAnsi(name);
                var find = (delegate* unmanaged<IntPtr, ulong>)Interop.NativeFunctions.Entity_FindByName;
                ulong id = find(namePtr);
                Marshal.FreeHGlobal(namePtr);

                if (id == 0) return null;
                return new Entity() { ID = id };
            }
        }

        public void ApplyLinearImpulse(Vector3 impulse, bool wakeUp = true)
        {
            unsafe
            {
                var applyImpulse = (delegate* unmanaged<ulong, Vector3*, byte, void>)Interop.NativeFunctions.Rigidbody_ApplyLinearImpulse;
                byte wakeFlag = (byte)(wakeUp ? 1 : 0);

                applyImpulse(ID, &impulse, wakeFlag);
            }
        }

        public Vector3 Forward
        {
            get
            {
                unsafe
                {
                    var func = (delegate* unmanaged<ulong, Vector3*, void>)Interop.NativeFunctions.Entity_GetForward;
                    Vector3 result;
                    func(ID, &result);
                    return result;
                }
            }
        }

        public Vector3 Right
        {
            get
            {
                unsafe
                {
                    var func = (delegate* unmanaged<ulong, Vector3*, void>)Interop.NativeFunctions.Entity_GetRight;
                    Vector3 result;
                    func(ID, &result);
                    return result;
                }
            }
        }

        public Vector3 Up
        {
            get
            {
                unsafe
                {
                    var func = (delegate* unmanaged<ulong, Vector3*, void>)Interop.NativeFunctions.Entity_GetUp;
                    Vector3 result;
                    func(ID, &result);
                    return result;
                }
            }
        }
    }
}