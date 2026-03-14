using System;
using System.Runtime.InteropServices;
using RXNScriptHost;

namespace RXNEngine
{
    public class Main
    {
        public Main()
        {
            Console.WriteLine("Core game scripts initialized!");
            LogNative("Hello from C#!");
        }

        private unsafe void LogNative(string message)
        {
            IntPtr stringPtr = Marshal.StringToHGlobalAnsi(message);

            try
            {
                var logFunc = (delegate* unmanaged<IntPtr, void>)Host.NativeFunctions.LogMessage;

                logFunc(stringPtr);
            }
            finally
            {
                Marshal.FreeHGlobal(stringPtr);
            }
        }
    }
}