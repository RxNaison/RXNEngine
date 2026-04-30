#pragma once

#include "Base.h"
#include "RXNEngine/Core/Log.h"
#include "RXNEngine/Core/Assert.h"
#include "Window.h"
#include "RXNEngine/Core/LayerStack.h"
#include "RXNEngine/Events/Event.h"
#include "RXNEngine/Events/ApplicationEvent.h"
#include "RXNEngine/ImGui/ImGuiLayer.h"
#include "RXNEngine/Core/Subsystem.h"
#include <unordered_map>
#include <typeindex>
#include <stdexcept>

namespace RXNEngine {

	struct ApplicationCommandLineArgs
	{
		int Count = 0;
		char** Args = nullptr;

		const char* operator[](int index) const
		{
			RXN_CORE_ASSERT(index < Count);
			return Args[index];
		}
	};

	class Application : public SubsystemRegistry
	{
	public:
		Application(const WindowProps& props = WindowProps(), ApplicationCommandLineArgs args = ApplicationCommandLineArgs());
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		inline Window& GetWindow() { return *m_Window; }
		inline ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }

		inline static Application& Get() { return *s_Instance; }

		ApplicationCommandLineArgs GetCommandLineArgs() const { return m_CommandLineArgs; }

		void Close() { m_Running = false; }

	private:
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);
		bool OnWindowMinimize(WindowMinimizeEvent& e);
	private:
		ApplicationCommandLineArgs m_CommandLineArgs;
		std::unique_ptr<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		bool m_Running = true;
		bool m_Minimized = false;

		LayerStack m_LayerStack;

		static Application* s_Instance;
	};

	Application* CreateApplication(ApplicationCommandLineArgs args);

}