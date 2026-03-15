using System;
using RXNScriptHost;

namespace RXNEngine
{
    public static class Input
    {
        public static bool IsKeyDown(KeyCode keycode)
        {
            unsafe
            {
                var isKeyDown = (delegate* unmanaged<KeyCode, bool>)Interop.NativeFunctions.Input_IsKeyDown;
                return isKeyDown(keycode);
            }
        }
    }
}