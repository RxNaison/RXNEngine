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

	class Application
	{
	public:
		Application(const WindowProps& props = WindowProps());
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		inline Window& GetWindow() { return *m_Window; }
		inline ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }

		inline static Application& Get() { return *s_Instance; }

		void Close() { m_Running = false; }

		template<typename T, typename... Args>
		void AddSubsystem(Args&&... args)
		{
			static_assert(std::is_base_of<Subsystem, T>::value, "System must inherit from RXNEngine::Subsystem!");

			Ref<T> subsystem = CreateRef<T>(std::forward<Args>(args)...);
			subsystem->Init();

			m_Subsystems[typeid(T)] = subsystem;
			m_SubsystemList.push_back(subsystem);
		}

		template<typename TInterface>
		void AddSubsystem(Ref<TInterface> instance)
		{
			static_assert(std::is_base_of<Subsystem, TInterface>::value, "System must inherit from RXNEngine::Subsystem!");

			instance->Init();

			m_Subsystems[typeid(TInterface)] = instance;
			m_SubsystemList.push_back(instance);
		}

		template<typename T>
		Ref<T> GetSubsystem()
		{
			auto it = m_Subsystems.find(typeid(T));

			if (it == m_Subsystems.end())
				throw std::runtime_error("Requested Subsystem is not registered!");

			return std::static_pointer_cast<T>(it->second);
		}
	private:
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);
		bool OnWindowMinimize(WindowMinimizeEvent& e);
	private:
		std::unique_ptr<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		bool m_Running = true;
		bool m_Minimized = false;

		std::unordered_map<std::type_index, Ref<Subsystem>> m_Subsystems;
		std::vector<Ref<Subsystem>> m_SubsystemList;

		LayerStack m_LayerStack;

		static Application* s_Instance;
	};

	Application* CreateApplication();

}