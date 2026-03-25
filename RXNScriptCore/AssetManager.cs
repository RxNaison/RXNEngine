using System;
using System.Runtime.InteropServices;
using RXNScriptHost;

namespace RXNEngine
{
    public static class AssetManager
    {
        public static void LoadMeshOntoEntityAsync(string filepath, Entity targetEntity)
        {
            unsafe
            {
                IntPtr pathPtr = Marshal.StringToHGlobalAnsi(filepath);
                ((delegate* unmanaged<IntPtr, ulong, void>)Interop.NativeFunctions.AssetManager_LoadMeshAsync)(pathPtr, targetEntity.ID);
                Marshal.FreeHGlobal(pathPtr);
            }
        }
    }
}