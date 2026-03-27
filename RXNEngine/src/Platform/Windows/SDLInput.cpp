#include "rxnpch.h"
#include "SDLInput.h"

namespace RXNEngine {

    Input* Input::s_Instance = new SDLInput();

    void SDLInput::InitImpl() {}
    void SDLInput::UpdateImpl() {}
    void SDLInput::ShutdownImpl()
    {
        for (int i = 0; i < 4; i++)
        {
            if (m_Gamepads[i])
            {
                SDL_CloseGamepad(m_Gamepads[i]);
                m_Gamepads[i] = nullptr;
            }
        }
    }

    void SDLInput::AddGamepad(SDL_JoystickID id)
    {
        SDL_Gamepad* pad = SDL_OpenGamepad(id);
        if (pad)
        {
            for (int i = 0; i < 4; i++) 
            {
                if (m_Gamepads[i] == nullptr) 
                {
                    m_Gamepads[i] = pad;
                    RXN_CORE_INFO("Player {0} Gamepad Connected: {1}", i, SDL_GetGamepadName(pad));
                    return;
                }
            }
            SDL_CloseGamepad(pad);
        }
    }

    void SDLInput::RemoveGamepad(SDL_JoystickID id)
    {
        for (int i = 0; i < 4; i++) 
        {
            if (m_Gamepads[i] && SDL_GetGamepadID(m_Gamepads[i]) == id) 
            {
                SDL_CloseGamepad(m_Gamepads[i]);
                m_Gamepads[i] = nullptr;
                RXN_CORE_INFO("Player {0} Gamepad Disconnected.", i);
                return;
            }
        }
    }

    bool SDLInput::IsKeyPressedImpl(int keycode)
    {
        int numKeys;
        const bool* state = SDL_GetKeyboardState(&numKeys);

        SDL_Scancode scancode = SDL_GetScancodeFromKey((SDL_Keycode)keycode, nullptr);

        if (scancode < 0 || scancode >= numKeys)
            return false;

        return state[scancode];
    }

    bool SDLInput::IsMouseButtonPressedImpl(int button)
    {
        float x, y;
        uint32_t state = SDL_GetMouseState(&x, &y);
        return (state & SDL_BUTTON_MASK(button)) != 0;
    }

    std::pair<float, float> SDLInput::GetMousePositionImpl()
    {
        float x, y;
        SDL_GetMouseState(&x, &y);
        return { x, y };
    }

    float SDLInput::GetMouseXImpl() { return GetMousePositionImpl().first; }
    float SDLInput::GetMouseYImpl() { return GetMousePositionImpl().second; }

    bool SDLInput::IsGamepadButtonPressedImpl(int gamepadID, GamepadButton button)
    {
        if (gamepadID < 0 || gamepadID >= 4 || m_Gamepads[gamepadID] == nullptr)
            return false;

        return SDL_GetGamepadButton(m_Gamepads[gamepadID], (SDL_GamepadButton)button);
    }

    float SDLInput::GetGamepadAxisImpl(int gamepadID, GamepadAxis axis)
    {
        if (gamepadID < 0 || gamepadID >= 4 || m_Gamepads[gamepadID] == nullptr)
            return 0.0f;

        int16_t rawValue = SDL_GetGamepadAxis(m_Gamepads[gamepadID], (SDL_GamepadAxis)axis);
        float value = (float)rawValue / 32767.0f;

        if (axis == GamepadAxis::LeftTrigger || axis == GamepadAxis::RightTrigger)
            return value;

        float deadzone = GetGamepadDeadzone();
        if (std::abs(value) < deadzone) return 0.0f;

        float sign = (value > 0.0f) ? 1.0f : -1.0f;
        return sign * ((std::abs(value) - deadzone) / (1.0f - deadzone));
    }

    void SDLInput::SetGamepadVibrationImpl(int gamepadID, float leftMotor, float rightMotor)
    {
        if (gamepadID < 0 || gamepadID >= 4 || m_Gamepads[gamepadID] == nullptr)
            return;

        leftMotor = std::fmax(0.0f, std::fmin(1.0f, leftMotor));
        rightMotor = std::fmax(0.0f, std::fmin(1.0f, rightMotor));

        Uint16 leftRumble = (Uint16)(leftMotor * 65535.0f);
        Uint16 rightRumble = (Uint16)(rightMotor * 65535.0f);

        SDL_RumbleGamepad(m_Gamepads[gamepadID], leftRumble, rightRumble, 100);
    }
}