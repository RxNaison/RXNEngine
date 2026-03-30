#pragma once
#include "RXNEngine/Core/Input.h"
#include <SDL3/SDL.h>
#include <map>

namespace RXNEngine {

    class SDLInput : public Input
    {
    public:
        void AddGamepad(SDL_JoystickID id);
        void RemoveGamepad(SDL_JoystickID id);

    protected:
        virtual void InitImpl() override;
        virtual void UpdateImpl() override;
        virtual void ShutdownImpl() override;

        virtual bool IsKeyPressedImpl(int keycode) override;
        virtual bool IsMouseButtonPressedImpl(int button) override;
        virtual std::pair<float, float> GetMousePositionImpl() override;
        virtual float GetMouseXImpl() override;
        virtual float GetMouseYImpl() override;
        virtual std::pair<float, float> GetMouseDeltaImpl() override;

        virtual bool IsGamepadButtonPressedImpl(int gamepadID, GamepadButton button) override;
        virtual float GetGamepadAxisImpl(int gamepadID, GamepadAxis axis) override;
        virtual void SetGamepadVibrationImpl(int gamepadID, float leftMotor, float rightMotor) override;

    private:
        SDL_Gamepad* m_Gamepads[4] = { nullptr, nullptr, nullptr, nullptr };
        float m_MouseDeltaX = 0.0f;
        float m_MouseDeltaY = 0.0f;
    };
}