#pragma once

#include "RXNEngine/Core/Base.h"
#include "RXNEngine/Core/KeyCodes.h"
#include "RXNEngine/Core/Subsystem.h"
#include <utility>

namespace RXNEngine {

    class Input : public Subsystem
    {
    public:
        virtual ~Input() = default;

        static Ref<Input> Create();

        virtual bool IsKeyPressed(int keycode) = 0;
        virtual bool IsMouseButtonPressed(int button) = 0;
        virtual std::pair<float, float> GetMousePosition() = 0;
        virtual float GetMouseX() = 0;
        virtual float GetMouseY() = 0;
        virtual std::pair<float, float> GetMouseDelta() = 0;

        virtual bool IsGamepadButtonPressed(int gamepadID, GamepadButton button) = 0;
        virtual float GetGamepadAxis(int gamepadID, GamepadAxis axis) = 0;

        inline float GetGamepadDeadzone() const { return m_GamepadDeadzone; }
        inline void SetGamepadDeadzone(float deadzone) { m_GamepadDeadzone = deadzone; }
        virtual void SetGamepadVibration(int gamepadID, float leftMotor, float rightMotor) = 0;

    protected:
        float m_GamepadDeadzone = 0.1f;
    };
}