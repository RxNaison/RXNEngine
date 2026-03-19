using System;
using System.Runtime.InteropServices;
using RXNScriptHost;

namespace RXNEngine
{
    [StructLayout(LayoutKind.Sequential)]
    public struct RaycastHit
    {
        public ulong EntityID;
        public Vector3 Position;
        public Vector3 Normal;
        public float Distance;
    }

    public static class Physics
    {
        public static bool Raycast(Vector3 origin, Vector3 direction, float maxDistance, out RaycastHit hitInfo, Entity? ignoreEntity = null)
        {
            unsafe
            {
                RaycastHit result = new RaycastHit();
                ulong ignoreID = ignoreEntity != null ? ignoreEntity.ID : 0;

                var func = (delegate* unmanaged<Vector3*, Vector3*, float, RaycastHit*, ulong, byte>)Interop.NativeFunctions.Physics_Raycast;

                byte hit = func(&origin, &direction, maxDistance, &result, ignoreID);
                hitInfo = result;
                return hit != 0;
            }
        }
    }
}