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

        public void Destroy()
        {
            unsafe
            {
                var destroy = (delegate* unmanaged<ulong, void>)Interop.NativeFunctions.Entity_Destroy;
                destroy(ID);
            }
        }

        public virtual void OnCreate() { }
        public virtual void OnUpdate(float deltaTime) { }

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

        public static Entity Instantiate(Entity original)
        {
            unsafe
            {
                var instantiate = (delegate* unmanaged<ulong, ulong>)Interop.NativeFunctions.Entity_InstantiatePrefab;
                ulong newID = instantiate(original.ID);
                return new Entity() { ID = newID };
            }
        }
    }
}