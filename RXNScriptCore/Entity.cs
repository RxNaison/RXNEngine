using System;
using System.Runtime.InteropServices;
using RXNScriptHost;

namespace RXNEngine
{
    public class Entity
    {
        public ulong ID { get; internal set; }

        protected Entity() { ID = 0; }
        internal Entity(ulong id) { ID = id; }

        #region Lifecycle & Instantiation
        public static Entity Instantiate()
        {
            unsafe { return new Entity() { ID = ((delegate* unmanaged<ulong>)Interop.NativeFunctions.Entity_Create)() }; }
        }

        public static Entity Instantiate(Entity original)
        {
            unsafe { return new Entity() { ID = ((delegate* unmanaged<ulong, ulong>)Interop.NativeFunctions.Entity_InstantiatePrefab)(original.ID) }; }
        }

        public void Destroy()
        {
            unsafe { ((delegate* unmanaged<ulong, void>)Interop.NativeFunctions.Entity_Destroy)(ID); }
        }

        public static Entity? FindEntityByName(string name)
        {
            unsafe
            {
                IntPtr namePtr = Marshal.StringToHGlobalAnsi(name);
                ulong id = ((delegate* unmanaged<IntPtr, ulong>)Interop.NativeFunctions.Entity_FindByName)(namePtr);
                Marshal.FreeHGlobal(namePtr);
                return id == 0 ? null : new Entity(id);
            }
        }
        #endregion

        #region Virtual Callbacks
        public virtual void OnCreate() { }
        public virtual void OnDestroy() { }
        public virtual void OnUpdate(float deltaTime) { }
        public virtual void OnFixedUpdate(float deltaTime) { }
        public virtual void OnCollisionEnter(Entity other) { }
        public virtual void OnCollisionExit(Entity other) { }
        public virtual void OnTriggerEnter(Entity other) { }
        public virtual void OnTriggerExit(Entity other) { }
        #endregion

        #region Transform Properties
        public Vector3 WorldPosition
        {
            get
            {
                unsafe
                {
                    Vector3 result;
                    ((delegate* unmanaged<ulong, Vector3*, void>)Interop.NativeFunctions.Entity_GetWorldPosition)(ID, &result);
                    return result;
                }
            }
        }
        public Vector3 Translation
        {
            get { unsafe { Vector3 res; ((delegate* unmanaged<ulong, Vector3*, void>)Interop.NativeFunctions.Entity_GetTranslation)(ID, &res); return res; } }
            set { unsafe { ((delegate* unmanaged<ulong, Vector3*, void>)Interop.NativeFunctions.Entity_SetTranslation)(ID, &value); } }
        }

        public Vector3 Rotation
        {
            get { unsafe { Vector3 res; ((delegate* unmanaged<ulong, Vector3*, void>)Interop.NativeFunctions.Entity_GetRotation)(ID, &res); return res; } }
            set { unsafe { ((delegate* unmanaged<ulong, Vector3*, void>)Interop.NativeFunctions.Entity_SetRotation)(ID, &value); } }
        }

        public Vector3 Forward => GetDirection(Interop.NativeFunctions.Entity_GetForward);
        public Vector3 Right => GetDirection(Interop.NativeFunctions.Entity_GetRight);
        public Vector3 Up => GetDirection(Interop.NativeFunctions.Entity_GetUp);

        private Vector3 GetDirection(IntPtr funcPtr)
        {
            unsafe
            {
                Vector3 result;
                ((delegate* unmanaged<ulong, Vector3*, void>)funcPtr)(ID, &result);
                return result;
            }
        }
        #endregion

        #region Physics Interop
        public void ApplyLinearImpulse(Vector3 impulse, bool wakeUp = true)
        {
            unsafe
            {
                byte wakeFlag = (byte)(wakeUp ? 1 : 0);
                ((delegate* unmanaged<ulong, Vector3*, byte, void>)Interop.NativeFunctions.Rigidbody_ApplyLinearImpulse)(ID, &impulse, wakeFlag);
            }
        }
        #endregion
    }
}