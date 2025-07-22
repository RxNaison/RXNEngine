#pragma once

#include "Base.h"

#include "Window.h"
#include "RXNEngine/Core/LayerStack.h"
#include "RXNEngine/Events/Event.h"
#include "RXNEngine/Events/ApplicationEvent.h"
#include "RXNEngine/ImGui/ImGuiLayer.h"

namespace RXNEngine {

	class Application
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
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);
		bool OnWindowMinimize(WindowMinimizeEvent& e);
	private:
		std::unique_ptr<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		bool m_Running = true;
		bool m_Minimized = false;
		LayerStack m_LayerStack;

		static Application* s_Instance;
	};

	Application* CreateApplication();

}