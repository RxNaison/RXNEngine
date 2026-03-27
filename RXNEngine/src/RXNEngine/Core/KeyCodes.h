#pragma once

#include <cstdint>

namespace RXNEngine
{
	enum KeyCode : uint32_t
	{
		Space = 32,
		Apostrophe = 39,
		Comma = 44,
		Minus = 45,
		Period = 46,
		Slash = 47,

		D0 = 48,
		D1 = 49,
		D2 = 50,
		D3 = 51,
		D4 = 52,
		D5 = 53,
		D6 = 54,
		D7 = 55,
		D8 = 56,
		D9 = 57,

		Semicolon = 59,
		Equal = 61,

		A = 97, B = 98, C = 99, D = 100,
		E = 101, F = 102, G = 103, H = 104,
		I = 105, J = 106, K = 107, L = 108,
		M = 109, N = 110, O = 111, P = 112,
		Q = 113, R = 114, S = 115, T = 116,
		U = 117, V = 118, W = 119, X = 120,
		Y = 121, Z = 122,

		LeftBracket = 91,
		Backslash = 92,
		RightBracket = 93,
		GraveAccent = 96,

		Backspace = 8,
		Tab = 9,
		Enter = 13,
		Escape = 27,
		Delete = 127,

		CapsLock = (1 << 30) | 57,
		F1 = (1 << 30) | 58,
		F2 = (1 << 30) | 59,
		F3 = (1 << 30) | 60,
		F4 = (1 << 30) | 61,
		F5 = (1 << 30) | 62,
		F6 = (1 << 30) | 63,
		F7 = (1 << 30) | 64,
		F8 = (1 << 30) | 65,
		F9 = (1 << 30) | 66,
		F10 = (1 << 30) | 67,
		F11 = (1 << 30) | 68,
		F12 = (1 << 30) | 69,

		PrintScreen = (1 << 30) | 70,
		ScrollLock = (1 << 30) | 71,
		Pause = (1 << 30) | 72,
		Insert = (1 << 30) | 73,
		Home = (1 << 30) | 74,
		PageUp = (1 << 30) | 75,
		End = (1 << 30) | 77,
		PageDown = (1 << 30) | 78,
		Right = (1 << 30) | 79,
		Left = (1 << 30) | 80,
		Down = (1 << 30) | 81,
		Up = (1 << 30) | 82,
		NumLock = (1 << 30) | 83,

		KPDivide = (1 << 30) | 84,
		KPMultiply = (1 << 30) | 85,
		KPSubtract = (1 << 30) | 86,
		KPAdd = (1 << 30) | 87,
		KPEnter = (1 << 30) | 88,
		KP1 = (1 << 30) | 89,
		KP2 = (1 << 30) | 90,
		KP3 = (1 << 30) | 91,
		KP4 = (1 << 30) | 92,
		KP5 = (1 << 30) | 93,
		KP6 = (1 << 30) | 94,
		KP7 = (1 << 30) | 95,
		KP8 = (1 << 30) | 96,
		KP9 = (1 << 30) | 97,
		KP0 = (1 << 30) | 98,
		KPDecimal = (1 << 30) | 99,
		KPEqual = (1 << 30) | 103,

		F13 = (1 << 30) | 104,
		F14 = (1 << 30) | 105,
		F15 = (1 << 30) | 106,
		F16 = (1 << 30) | 107,
		F17 = (1 << 30) | 108,
		F18 = (1 << 30) | 109,
		F19 = (1 << 30) | 110,
		F20 = (1 << 30) | 111,
		F21 = (1 << 30) | 112,
		F22 = (1 << 30) | 113,
		F23 = (1 << 30) | 114,
		F24 = (1 << 30) | 115,

		LeftControl = (1 << 30) | 224,
		LeftShift = (1 << 30) | 225,
		LeftAlt = (1 << 30) | 226,
		LeftSuper = (1 << 30) | 227,
		RightControl = (1 << 30) | 228,
		RightShift = (1 << 30) | 229,
		RightAlt = (1 << 30) | 230,
		RightSuper = (1 << 30) | 231,
		Menu = (1 << 30) | 101
	};

	enum MouseCode : uint32_t
	{
		ButtonLeft = 1,
		ButtonMiddle = 2,
		ButtonRight = 3,
		ButtonX1 = 4,
		ButtonX2 = 5,

		Button0 = 1,
		Button1 = 3,
		Button2 = 2,
		Button3 = 4,
		Button4 = 5
	};

	enum class GamepadButton : uint32_t
	{
		A = 0,
		B = 1,
		X = 2,
		Y = 3,

		Cross = 0,
		Circle = 1,
		Square = 2,
		Triangle = 3,

		Back = 4,
		Guide = 5,
		Start = 6,
		LeftThumb = 7,
		RightThumb = 8,
		LeftBumper = 9,
		RightBumper = 10,

		DPadUp = 11,
		DPadDown = 12,
		DPadLeft = 13,
		DPadRight = 14
	};

	enum GamepadAxis : uint32_t
	{
		LeftX = 0,
		LeftY = 1,
		RightX = 2,
		RightY = 3,
		LeftTrigger = 4,
		RightTrigger = 5
	};
}