#pragma once

#include "Base.h"
#include "KeyCodes.h"

namespace RXNEngine {

	class Input
	{
	public:
		static void Init() { s_Instance->InitImpl(); }
		static void Update() { s_Instance->UpdateImpl(); }
		static void Shutdown() { s_Instance->ShutdownImpl(); }

		inline static Input* Get() { return s_Instance; }

		inline static bool IsKeyPressed(int keycode) { return s_Instance->IsKeyPressedImpl(keycode); }
		inline static bool IsMouseButtonPressed(int button) { return s_Instance->IsMouseButtonPressedImpl(button); }
		inline static std::pair<float, float> GetMousePosition() { return s_Instance->GetMousePositionImpl(); }
		inline static float GetMouseX() { return s_Instance->GetMouseXImpl(); }
		inline static float GetMouseY() { return s_Instance->GetMouseYImpl(); }
		inline static std::pair<float, float> GetMouseDelta() { return s_Instance->GetMouseDeltaImpl(); }

		inline static bool IsGamepadButtonPressed(int gamepadID, GamepadButton button) { return s_Instance->IsGamepadButtonPressedImpl(gamepadID, button); }
		inline static float GetGamepadAxis(int gamepadID, GamepadAxis axis) { return s_Instance->GetGamepadAxisImpl(gamepadID, axis); }

		inline static float GetGamepadDeadzone() { return s_Instance->m_GamepadDeadzone; }
		inline static void SetGamepadDeadzone(float deadzone) { s_Instance->m_GamepadDeadzone = deadzone; }
		inline static void SetGamepadVibration(int gamepadID, float leftMotor, float rightMotor) { s_Instance->SetGamepadVibrationImpl(gamepadID, leftMotor, rightMotor); }
	protected:
		virtual void InitImpl() = 0;
		virtual void UpdateImpl() = 0;
		virtual void ShutdownImpl() = 0;

		virtual bool IsKeyPressedImpl(int keycode) = 0;
		virtual bool IsMouseButtonPressedImpl(int button) = 0;
		virtual std::pair<float, float> GetMousePositionImpl() = 0;
		virtual float GetMouseXImpl() = 0;
		virtual float GetMouseYImpl() = 0;
		virtual std::pair<float, float> GetMouseDeltaImpl() = 0;

		virtual bool IsGamepadButtonPressedImpl(int gamepadID, GamepadButton button) = 0;
		virtual float GetGamepadAxisImpl(int gamepadID, GamepadAxis axis) = 0;
		virtual void SetGamepadVibrationImpl(int gamepadID, float leftMotor, float rightMotor) = 0;
	private:
		static Input* s_Instance;
		float m_GamepadDeadzone = 0.1f;
	};
}