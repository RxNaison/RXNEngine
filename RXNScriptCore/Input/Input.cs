using System;
using RXNScriptHost;

namespace RXNEngine
{
    public enum CursorMode { Normal = 0, Hidden = 1, Locked = 2 }
    public static class Input
    {
        public static bool IsKeyDown(KeyCode keycode)
        {
            unsafe
            {
                var isKeyDown = (delegate* unmanaged<KeyCode, byte>)Interop.NativeFunctions.Input_IsKeyDown;
                return isKeyDown(keycode) != 0;
            }
        }

        public static Vector2 GetMousePosition()
        {
            unsafe
            {
                var func = (delegate* unmanaged<Vector2*, void>)Interop.NativeFunctions.Input_GetMousePosition;
                Vector2 result;
                func(&result);
                return result;
            }
        }

        public static void SetCursorMode(CursorMode mode)
        {
            unsafe
            {
                var func = (delegate* unmanaged<int, void>)Interop.NativeFunctions.Input_SetCursorMode;
                func((int)mode);
            }
        }
    }
}