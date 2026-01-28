#pragma once

#include "rxnpch.h"

#include "RXNEngine/Core/Base.h"
#include "RXNEngine/Events/Event.h"

namespace RXNEngine {

	enum class WindowMode
	{
		Windowed,
		Maximized,
		Borderless,
		Fullscreen
	};

	struct WindowProps
	{
		std::string Title;
		uint32_t Width;
		uint32_t Height;
		WindowMode Mode;

		WindowProps(const std::string& title = "RXNEngine",
			uint32_t width = 1600, uint32_t height = 900, WindowMode mode = WindowMode::Windowed)
			: Title(title), Width(width), Height(height), Mode(mode)
		{
		}
	};

	class Window
	{
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		virtual ~Window() {}

		virtual void OnUpdate() = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
		virtual void SetVSync(bool enabled) = 0;
		virtual bool IsVSync() const = 0;

		virtual void* GetNativeWindow() const = 0;

		static Window* Create(const WindowProps& props = WindowProps());
	};

}