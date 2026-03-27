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

        public static float GamepadDeadzone
        {
            get
            {
                unsafe { return ((delegate* unmanaged<float>)Interop.NativeFunctions.Input_GetGamepadDeadzone)(); }
            }
            set
            {
                unsafe { ((delegate* unmanaged<float, void>)Interop.NativeFunctions.Input_SetGamepadDeadzone)(value); }
            }
        }

        public static bool IsGamepadButtonDown(int gamepadID, GamepadButton button)
        {
            unsafe
            {
                byte result = ((delegate* unmanaged<int, int, byte>)Interop.NativeFunctions.Input_IsGamepadButtonDown)(gamepadID, (int)button);
                return result != 0;
            }
        }

        public static float GetGamepadAxis(int gamepadID, GamepadAxis axis)
        {
            unsafe
            {
                return ((delegate* unmanaged<int, int, float>)Interop.NativeFunctions.Input_GetGamepadAxis)(gamepadID, (int)axis);
            }
        }
        public static void SetGamepadVibration(int gamepadID, float leftMotor, float rightMotor)
        {
            unsafe
            {
                ((delegate* unmanaged<int, float, float, void>)Interop.NativeFunctions.Input_SetGamepadVibration)(gamepadID, leftMotor, rightMotor);
            }
        }
    }
}