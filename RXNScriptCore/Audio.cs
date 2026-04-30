using System;
using System.Runtime.InteropServices;
using RXNScriptHost;

namespace RXNEngine
{
    public static class Audio
    {
        public static void PlayOneShot(string filepath, float volume = 1.0f)
        {
            unsafe
            {
                IntPtr pathPtr = Marshal.StringToHGlobalAnsi(filepath);
                ((delegate* unmanaged<IntPtr, float, void>)Interop.NativeFunctions.NativeAudio_PlayOneShot)(pathPtr, volume);
                Marshal.FreeHGlobal(pathPtr);
            }
        }

        public static void LoadBank(string filepath)
        {
            unsafe
            {
                IntPtr pathPtr = Marshal.StringToHGlobalAnsi(filepath);
                ((delegate* unmanaged<IntPtr, void>)Interop.NativeFunctions.NativeAudio_LoadBank)(pathPtr);
                Marshal.FreeHGlobal(pathPtr);
            }
        }

        public static void UnloadBank(string filepath)
        {
            unsafe
            {
                IntPtr pathPtr = Marshal.StringToHGlobalAnsi(filepath);
                ((delegate* unmanaged<IntPtr, void>)Interop.NativeFunctions.NativeAudio_UnloadBank)(pathPtr);
                Marshal.FreeHGlobal(pathPtr);
            }
        }
    }
}