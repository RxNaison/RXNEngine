using System;
using RXNScriptHost;

namespace RXNEngine
{
    public abstract class Entity
    {
        public ulong ID { get; internal set; }
        protected Entity() { ID = 0; }

        internal Entity(ulong id)
        {
            ID = id;
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
    }
}