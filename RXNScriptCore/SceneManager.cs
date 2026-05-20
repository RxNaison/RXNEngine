using System;
using System.Runtime.InteropServices;

namespace RXNEngine
{
    public static class SceneManager
    {
        public static void LoadScene(string path)
        {
            unsafe
            {
                IntPtr pathPtr = Marshal.StringToHGlobalAnsi(path);
                ((delegate* unmanaged<IntPtr, void>)RXNScriptHost.Interop.NativeFunctions.SceneManager_LoadScene)(pathPtr);
                Marshal.FreeHGlobal(pathPtr);
            }
        }
    }
}
