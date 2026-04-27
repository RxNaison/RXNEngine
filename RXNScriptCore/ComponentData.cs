using System.Runtime.InteropServices;

namespace RXNEngine
{
    [StructLayout(LayoutKind.Sequential)]
    public struct TransformData
    {
        public Vector3 Translation;
        public Vector3 Rotation;
        public Vector3 Scale;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct DirectionalLightData
    {
        public Vector3 Color;
        public float Intensity;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct PointLightData
    {
        public Vector3 Color;
        public float Intensity;
        public float Radius;
        public float Falloff;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SpotLightData
    {
        public Vector3 Color;
        public float Intensity;
        public float Radius;
        public float Falloff;
        public float InnerAngle;
        public float OuterAngle;
        public float CookieSize;
    }

    public enum BodyType { Static = 0, Dynamic, Kinematic }

    [StructLayout(LayoutKind.Sequential)]
    public struct RigidbodyData
    {
        public BodyType Type;
        public float Mass;
        public float LinearDrag;
        public float AngularDrag;
        public bool FixedRotation;
        public bool UseCCD;
        public float CCDVelocityThreshold;

        internal IntPtr RuntimeActor;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct BoxColliderData
    {
        public Vector3 HalfExtents;
        public Vector3 Offset;
        public bool IsTrigger;

        internal IntPtr RuntimeShape;
        internal IntPtr RuntimeMaterial;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SphereColliderComponentData
    {
        float Radius;
        Vector3 Offset;
        bool IsTrigger;

        internal IntPtr RuntimeShape;
        internal IntPtr RuntimeMaterial;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct CapsuleColliderComponentData
    {
        float Radius;
        float Height;
        Vector3 Offset;
        bool IsTrigger;

        internal IntPtr RuntimeShape;
        internal IntPtr RuntimeMaterial;
    }

}
