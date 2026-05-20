using RXNScriptHost;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace RXNEngine
{
    public abstract class Component
    {
        public Entity EntityHandle { get; internal set; } = null!;
    }

    public class IDComponent : Component
    {
        public ulong ID
        {
            get
            {
                return EntityHandle.ID;
            }
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

    public class SpotLightComponent : Component
    {
        public SpotLightData Data
        {
            get
            {
                unsafe
                {
                    SpotLightData res;
                    ((delegate* unmanaged<ulong, SpotLightData*, void>)Interop.NativeFunctions.NativeSpotLight_Get)(EntityHandle.ID, &res);
                    return res;
                }
            }
            set
            {
                unsafe
                {
                    ((delegate* unmanaged<ulong, SpotLightData*, void>)Interop.NativeFunctions.NativeSpotLight_Set)(EntityHandle.ID, &value);
                }
            }
        }

        // --- Video Controls ---
        public void PlayVideo()
        {
            unsafe
            {
                ((delegate* unmanaged<ulong, void>)Interop.NativeFunctions.NativeSpotLight_VideoPlay)(EntityHandle.ID);
            }
        }

        public void PauseVideo()
        {
            unsafe 
            {
                ((delegate* unmanaged<ulong, void>)Interop.NativeFunctions.NativeSpotLight_VideoPause)(EntityHandle.ID);
            }
        }

        public void RewindVideo()
        {
            unsafe
            {
                ((delegate* unmanaged<ulong, void>)Interop.NativeFunctions.NativeSpotLight_VideoRewind)(EntityHandle.ID);
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

    [Flags]
    public enum CollisionFlags : byte
    {
        None = 0,
        Sides = 1 << 0,
        Up = 1 << 1,
        Down = 1 << 2
    }

    public class CharacterControllerComponent : Component
    {
        public CollisionFlags Move(Vector3 displacement, float deltaTime)
        {
            unsafe
            {
                var func = (delegate* unmanaged<ulong, Vector3*, float, byte>)Interop.NativeFunctions.CharacterController_Move;
                return (CollisionFlags)func(EntityHandle.ID, &displacement, deltaTime);
            }
        }
    }

    public class AudioSourceComponent : Component
    {
        public void Play()
        {
            unsafe
            {
                ((delegate* unmanaged<ulong, void>)Interop.NativeFunctions.NativeAudioSource_Play)(EntityHandle.ID);
            }
        }

        public void Stop()
        {
            unsafe
            {
                ((delegate* unmanaged<ulong, void>)Interop.NativeFunctions.NativeAudioSource_Stop)(EntityHandle.ID);
            }
        }

        public float Volume
        {
            set
            {
                unsafe
                {
                    ((delegate* unmanaged<ulong, float, void>)Interop.NativeFunctions.NativeAudioSource_SetVolume)(EntityHandle.ID, value);
                }
            }
        }

        public void SetParameter(string paramName, float value)
        {
            unsafe
            {
                IntPtr namePtr = Marshal.StringToHGlobalAnsi(paramName);
                ((delegate* unmanaged<ulong, IntPtr, float, void>)Interop.NativeFunctions.NativeAudioSource_SetParameter)(EntityHandle.ID, namePtr, value);
                Marshal.FreeHGlobal(namePtr);
            }
        }
    }

    public class UICanvasComponent : Component
    {
        public bool Active
        {
            get
            {
                unsafe
                {
                    return ((delegate* unmanaged<ulong, bool>)Interop.NativeFunctions.Entity_UICanvas_GetActive)(EntityHandle.ID); 
                }
            }
            set
            {
                unsafe
                {
                    ((delegate* unmanaged<ulong, bool, void>)Interop.NativeFunctions.Entity_UICanvas_SetActive)(EntityHandle.ID, value);
                }
            }
        }
    }

    public class UITransformComponent : Component
    {
        public UITransformData Data
        {
            get
            {
                unsafe
                {
                    UITransformData res; ((delegate* unmanaged<ulong, UITransformData*, void>)Interop.NativeFunctions.Entity_UITransform_Get)(EntityHandle.ID, &res);
                    return res;
                }
            }
            set
            {
                unsafe 
                {
                    ((delegate* unmanaged<ulong, UITransformData*, void>)Interop.NativeFunctions.Entity_UITransform_Set)(EntityHandle.ID, &value);
                }
            }
        }
    }

    public class UIImageComponent : Component
    {
        public Vector4 TintColor
        {
            get
            {
                unsafe
                {
                    Vector4 res; ((delegate* unmanaged<ulong, Vector4*, void>)Interop.NativeFunctions.Entity_UIImage_GetTintColor)(EntityHandle.ID, &res);
                    return res;
                }
            }
            set
            {
                unsafe
                {
                    ((delegate* unmanaged<ulong, Vector4*, void>)Interop.NativeFunctions.Entity_UIImage_SetTintColor)(EntityHandle.ID, &value);
                }
            }
        }

        public string TextureAssetPath
        {
            get
            {
                unsafe
                {
                    IntPtr buffer = Marshal.AllocHGlobal(256);
                    ((delegate* unmanaged<ulong, IntPtr, uint, void>)Interop.NativeFunctions.Entity_UIImage_GetTexture)(EntityHandle.ID, buffer, 256);
                    string result = Marshal.PtrToStringAnsi(buffer) ?? "";
                    Marshal.FreeHGlobal(buffer);
                    return result;
                }
            }
            set
            {
                unsafe
                {
                    IntPtr ptr = Marshal.StringToHGlobalAnsi(value);
                    ((delegate* unmanaged<ulong, IntPtr, void>)Interop.NativeFunctions.Entity_UIImage_SetTexture)(EntityHandle.ID, ptr);
                    Marshal.FreeHGlobal(ptr);
                }
            }
        }
    }

    public class UITextComponent : Component
    {
        public string Text
        {
            get
            {
                unsafe
                {
                    IntPtr buffer = Marshal.AllocHGlobal(2048);
                    ((delegate* unmanaged<ulong, IntPtr, uint, void>)Interop.NativeFunctions.Entity_UIText_GetText)(EntityHandle.ID, buffer, 2048);
                    string result = Marshal.PtrToStringAnsi(buffer) ?? "";
                    Marshal.FreeHGlobal(buffer);
                    return result;
                }
            }
            set
            {
                unsafe
                {
                    IntPtr ptr = Marshal.StringToHGlobalAnsi(value);
                    ((delegate* unmanaged<ulong, IntPtr, void>)Interop.NativeFunctions.Entity_UIText_SetText)(EntityHandle.ID, ptr);
                    Marshal.FreeHGlobal(ptr);
                }
            }
        }

        public UITextData Data
        {
            get
            {
                unsafe
                {
                    UITextData res; ((delegate* unmanaged<ulong, UITextData*, void>)Interop.NativeFunctions.Entity_UIText_GetData)(EntityHandle.ID, &res);
                    return res;
                }
            }
            set
            {
                unsafe
                {
                    ((delegate* unmanaged<ulong, UITextData*, void>)Interop.NativeFunctions.Entity_UIText_SetData)(EntityHandle.ID, &value);
                }
            }
        }
    }
}