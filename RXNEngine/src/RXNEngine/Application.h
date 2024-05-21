#pragma once

#include "Core.h"

#include "Window.h"
#include "RXNEngine/LayerStack.h"
#include "RXNEngine/Events/Event.h"
#include "RXNEngine/Events/ApplicationEvent.h"

namespace RXNEngine {

	class RXN_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		inline Window& GetWindow() { return *m_Window; }

		inline static Application& Get() { return *s_Instance; }
	private:
		bool OnWindowClosed(WindowCloseEvent& e);

		std::unique_ptr<Window> m_Window;
		bool m_Running = true;
		LayerStack m_LayerStack;

		static Application* s_Instance;
	};

	Application* CreateApplication();

}