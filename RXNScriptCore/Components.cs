using RXNScriptHost;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace RXNEngine
{
    public abstract class Component
    {
        public Entity EntityHandle { get; internal set; }
    }

    public class IDComponent : Component
    {
        public ulong ID
        {
            get { return EntityHandle.ID; }
        }
    }

    public class TagComponent : Component
    {
        public string Tag
        {
            get
            {
                unsafe
                {
                    IntPtr buffer = Marshal.AllocHGlobal(256);
                    ((delegate* unmanaged<ulong, IntPtr, uint, void>)Interop.NativeFunctions.Entity_Tag_Get)(EntityHandle.ID, buffer, 256);

                    string result = Marshal.PtrToStringAnsi(buffer) ?? "";
                    Marshal.FreeHGlobal(buffer);
                    return result;
                }
            }
            set
            {
                unsafe
                {
                    IntPtr tagPtr = Marshal.StringToHGlobalAnsi(value);
                    ((delegate* unmanaged<ulong, IntPtr, void>)Interop.NativeFunctions.Entity_Tag_Set)(EntityHandle.ID, tagPtr);
                    Marshal.FreeHGlobal(tagPtr);
                }
            }
        }
    }

    public class TransformComponent : Component
    {
        public TransformData Data
        {
            get
            {
                unsafe
                {
                    TransformData res; ((delegate* unmanaged<ulong, TransformData*, void>)Interop.NativeFunctions.Entity_Transform_Get)(EntityHandle.ID, &res);
                    return res;
                }
            }
            set
            { 
                unsafe
                {
                    ((delegate* unmanaged<ulong, TransformData*, void>)Interop.NativeFunctions.Entity_Transform_Set)(EntityHandle.ID, &value);
                }
            }
        }
    }

    public class RelationshipComponent : Component
    {
        public Entity? Parent
        {
            get
            {
                unsafe
                {
                    ulong parentID = ((delegate* unmanaged<ulong, ulong>)Interop.NativeFunctions.Entity_Relationship_GetParent)(EntityHandle.ID);
                    if (parentID == 0) return null;

                    Entity parentEntity = new Entity(0);
                    parentEntity.ID = parentID;
                    return parentEntity;
                }
            }
            set
            {
                unsafe
                {
                    ulong parentID = value != null ? value.ID : 0;
                    ((delegate* unmanaged<ulong, ulong, void>)Interop.NativeFunctions.Entity_Relationship_SetParent)(EntityHandle.ID, parentID);
                }
            }
        }
    }

    public class StaticMeshComponent : Component
    {
        public string AssetPath
        {
            get
            {
                unsafe
                {
                    IntPtr buffer = Marshal.AllocHGlobal(256);
                    ((delegate* unmanaged<ulong, IntPtr, uint, void>)Interop.NativeFunctions.Entity_StaticMesh_GetAssetPath)(EntityHandle.ID, buffer, 256);

                    string result = Marshal.PtrToStringAnsi(buffer) ?? "";
                    Marshal.FreeHGlobal(buffer);
                    return result;
                }
            }
            set
            {
                unsafe
                {
                    IntPtr tagPtr = Marshal.StringToHGlobalAnsi(value);
                    ((delegate* unmanaged<ulong, IntPtr, void>)Interop.NativeFunctions.Entity_StaticMesh_SetAssetPath)(EntityHandle.ID, tagPtr);
                    Marshal.FreeHGlobal(tagPtr);
                }
            }
        }
    }

    public class DirectionalLightComponent : Component
    {
        public DirectionalLightData Data
        {
            get
            {
                unsafe
                {
                    DirectionalLightData res;
                    ((delegate* unmanaged<ulong, DirectionalLightData*, void>)Interop.NativeFunctions.DirLight_Get)(EntityHandle.ID, &res);
                    return res;
                }
            }
            set
            {
                unsafe
                {
                    ((delegate* unmanaged<ulong, DirectionalLightData*, void>)Interop.NativeFunctions.DirLight_Set)(EntityHandle.ID, &value);
                }
            }
        }
    }

    public class PointLightComponent : Component
    {
        public PointLightData Data
        {
            get
            {
                unsafe
                {
                    PointLightData res;
                    ((delegate* unmanaged<ulong, PointLightData*, void>)Interop.NativeFunctions.PointLight_Get)(EntityHandle.ID, &res);
                    return res;
                }
            }
            set
            {
                unsafe
                {
                    ((delegate* unmanaged<ulong, PointLightData*, void>)Interop.NativeFunctions.PointLight_Set)(EntityHandle.ID, &value);
                }
            }
        }
    }

    public class ScriptComponent : Component
    {
        public string ScriptPath
        {
            get
            {
                unsafe
                {
                    IntPtr buffer = Marshal.AllocHGlobal(256);
                    ((delegate* unmanaged<ulong, IntPtr, uint, void>)Interop.NativeFunctions.Entity_Script_Get)(EntityHandle.ID, buffer, 256);

                    string result = Marshal.PtrToStringAnsi(buffer) ?? "";
                    Marshal.FreeHGlobal(buffer);
                    return result;
                }
            }
            set
            {
                unsafe
                {
                    IntPtr tagPtr = Marshal.StringToHGlobalAnsi(value);
                    ((delegate* unmanaged<ulong, IntPtr, void>)Interop.NativeFunctions.Entity_Script_Set)(EntityHandle.ID, tagPtr);
                    Marshal.FreeHGlobal(tagPtr);
                }
            }
        }
    }

    public class RigidbodyComponent : Component
    {
        public RigidbodyData Data
        {
            get
            {
                unsafe
                {
                    RigidbodyData res;
                    ((delegate* unmanaged<ulong, RigidbodyData*, void>)Interop.NativeFunctions.Entity_Rigidbody_Get)(EntityHandle.ID, &res);
                    return res;
                }
            }
            set
            { 
                unsafe 
                {
                    ((delegate* unmanaged<ulong, RigidbodyData*, void>)Interop.NativeFunctions.Entity_Rigidbody_Set)(EntityHandle.ID, &value);
                }
            }
        }
    }

    public class BoxColliderComponent : Component
    {
        public BoxColliderData Data
        {
            get
            {
                unsafe
                {
                    BoxColliderData res;
                    ((delegate* unmanaged<ulong, BoxColliderData*, void>)Interop.NativeFunctions.Entity_BoxCollider_Get)(EntityHandle.ID, &res);
                    return res;
                }
            }
            set
            { 
                unsafe
                {
                    ((delegate* unmanaged<ulong, BoxColliderData*, void>)Interop.NativeFunctions.Entity_BoxCollider_Set)(EntityHandle.ID, &value);
                }
            }
        }
    }

    public class SphereColliderComponent : Component
    {
        public SphereColliderComponentData Data
        {
            get
            {
                unsafe
                {
                    SphereColliderComponentData res;
                    ((delegate* unmanaged<ulong, SphereColliderComponentData*, void>)Interop.NativeFunctions.Entity_SphereCollider_Get)(EntityHandle.ID, &res);
                    return res;
                }
            }
            set
            {
                unsafe
                {
                    ((delegate* unmanaged<ulong, SphereColliderComponentData*, void>)Interop.NativeFunctions.Entity_SphereCollider_Set)(EntityHandle.ID, &value);
                }
            }
        }
    }

    public class CapsuleColliderComponent : Component
    {
        public CapsuleColliderComponentData Data
        {
            get
            {
                unsafe
                {
                    CapsuleColliderComponentData res;
                    ((delegate* unmanaged<ulong, CapsuleColliderComponentData*, void>)Interop.NativeFunctions.Entity_CapsuleCollider_Get)(EntityHandle.ID, &res);
                    return res;
                }
            }
            set
            {
                unsafe
                {
                    ((delegate* unmanaged<ulong, CapsuleColliderComponentData*, void>)Interop.NativeFunctions.Entity_CapsuleCollider_Set)(EntityHandle.ID, &value);
                }
            }
        }
    }

}