#include "rxnpch.h"
#include "Application.h"

#include "RXNEngine/Renderer/Renderer.h"
#include "RXNEngine/Renderer/RenderCommand.h"
#include "RXNEngine/Core/Time.h"
#include "RXNEngine/Core/Input.h"
#include "RXNEngine/Core/JobSystem.h"
#include "RXNEngine/Physics/PhysicsSystem.h"
#include "RXNEngine/Scripting/ScriptEngine.h"
#include "RXNEngine/Asset/AssetManager.h"

namespace RXNEngine {

	Application* Application::s_Instance = nullptr;

	Application::Application(const WindowProps& props)
	{
		RXN_CORE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		AddSubsystem<Log>();

		m_Window = std::unique_ptr<Window>(Window::Create(props));
		m_Window->SetEventCallback([this](Event& e) { OnEvent(e); });

		AddSubsystem<JobSystem>();
		AddSubsystem<Renderer>();
		AddSubsystem<Input>(Input::Create());
		AddSubsystem<PhysicsSystem>();
		AddSubsystem<ScriptEngine>();
		AddSubsystem<AssetManager>();

		m_ImGuiLayer = new ImGuiLayer();
		PushOverlay(m_ImGuiLayer);
	}

	Application::~Application()
	{
		for (Layer* layer : m_LayerStack)
		{
			layer->OnDetach();
			delete layer;
		}

		if (m_Subsystems.find(typeid(JobSystem)) != m_Subsystems.end())
			GetSubsystem<JobSystem>()->Shutdown();

		for (auto it = m_SubsystemList.rbegin(); it != m_SubsystemList.rend(); ++it)
		{
			if ((*it) != m_Subsystems[typeid(JobSystem)])
				(*it)->Shutdown();
		}

		m_Subsystems.clear();
		m_SubsystemList.clear();
	}

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* layer)
	{
		m_LayerStack.PushOverlay(layer);
		layer->OnAttach();
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& e)->bool { return OnWindowClose(e); });
		dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e)->bool { return OnWindowResize(e); });
		dispatcher.Dispatch<WindowMinimizeEvent>([this](WindowMinimizeEvent& e)->bool { return OnWindowMinimize(e); });

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
		{
			(*--it)->OnEvent(e);
			if (e.Handled)
				break;
		}
	}

	void Application::Run()
	{
		while (m_Running)
		{
			OPTICK_FRAME("MainThread");

			Time::Get().OnFrameStart();

			if (!m_Minimized)
			{
				for (auto& subsystem : m_SubsystemList)
					subsystem->Update(Time::Get().GetDeltaTime());

				while (Time::Get().ShouldRunFixedUpdate())
				{
					for (Layer* layer : m_LayerStack)
						layer->OnFixedUpdate(Time::Get().GetFixedDeltaTime());
				}

				for (Layer* layer : m_LayerStack)
					layer->OnUpdate(Time::Get().GetDeltaTime());
			}

			m_ImGuiLayer->Begin();
			for (Layer* layer : m_LayerStack)
				layer->OnImGuiRenderer();
			m_ImGuiLayer->End();

			m_Window->OnUpdate();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}
	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		GetSubsystem<Renderer>()->OnWindowResize(e.GetWidth(), e.GetHeight());
		return false;
	}
	bool Application::OnWindowMinimize(WindowMinimizeEvent& e)
	{
		m_Minimized = e.GetMinimizeState();
		return true;
	}
}