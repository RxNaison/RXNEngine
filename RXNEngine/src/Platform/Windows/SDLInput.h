#pragma once
#include "RXNEngine/Core/Input.h"
#include <SDL3/SDL.h>

namespace RXNEngine {

    class SDLInput : public Input
    {
    public:
        void AddGamepad(SDL_JoystickID id);
        void RemoveGamepad(SDL_JoystickID id);

    protected:
        virtual void Init() override;
        virtual void Update(float deltaTime) override;
        virtual void Shutdown() override;

        virtual bool IsKeyPressed(int keycode) override;
        virtual bool IsMouseButtonPressed(int button) override;
        virtual std::pair<float, float> GetMousePosition() override;
        virtual float GetMouseX() override;
        virtual float GetMouseY() override;
        virtual std::pair<float, float> GetMouseDelta() override;

        virtual bool IsGamepadButtonPressed(int gamepadID, GamepadButton button) override;
        virtual float GetGamepadAxis(int gamepadID, GamepadAxis axis) override;
        virtual void SetGamepadVibration(int gamepadID, float leftMotor, float rightMotor) override;

    private:
        SDL_Gamepad* m_Gamepads[4] = { nullptr, nullptr, nullptr, nullptr };
        float m_MouseDeltaX = 0.0f;
        float m_MouseDeltaY = 0.0f;
    };
}