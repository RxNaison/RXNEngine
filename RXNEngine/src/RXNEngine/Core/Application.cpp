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
#include "RXNEngine/Audio/AudioSystem.h"
#include "RXNEngine/Scene/SceneManager.h"
#include "RXNEngine/Core/VFSSystem.h"

namespace RXNEngine {

	Application* Application::s_Instance = nullptr;

	Application::Application(const WindowProps& props, ApplicationCommandLineArgs args, bool enableImGui)
		: m_CommandLineArgs(args), m_ImGuiEnabled(enableImGui)
	{
		RXN_PROFILE_SCOPE();

		RXN_CORE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		AddSubsystem<Log>();
		AddSubsystem<VFSSystem>();

		WindowProps finalProps = props;
		auto vfs = GetSubsystem<VFSSystem>();
		if (vfs)
		{
			if (vfs->GetBakedWindowWidth() > 0 && vfs->GetBakedWindowHeight() > 0)
			{
				finalProps.Title = vfs->GetBakedWindowTitle();
				finalProps.Width = vfs->GetBakedWindowWidth();
				finalProps.Height = vfs->GetBakedWindowHeight();
				finalProps.Mode = (WindowMode)vfs->GetBakedWindowMode();
			}
		}

#if !defined(RXN_DIST)
		if (std::filesystem::exists("app.ini"))
		{
			std::ifstream ini("app.ini");
			std::string line;
			while (std::getline(ini, line))
			{
				line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
				size_t delim = line.find('=');
				if (delim == std::string::npos)
					continue;

				std::string key = line.substr(0, delim);
				std::string value = line.substr(delim + 1);

				auto trim = [](std::string& s)
					{
						s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
						s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
					};
				trim(key);
				trim(value);

				if (key == "window-title" || key == "title")
				{
					finalProps.Title = value;
				}
				else if (key == "window-width" || key == "width")
				{
					try
					{ 
						finalProps.Width = std::stoi(value);
					}
					catch (...) {}
				}
				else if (key == "window-height" || key == "height")
				{
					try 
					{ 
						finalProps.Height = std::stoi(value);
					} 
					catch (...) {}
				}
				else if (key == "window-mode" || key == "mode")
				{
					if (value == "Fullscreen" || value == "fullscreen" || value == "3")
						finalProps.Mode = WindowMode::Fullscreen;
					else if (value == "Borderless" || value == "borderless" || value == "2")
						finalProps.Mode = WindowMode::Borderless;
					else if (value == "Maximized" || value == "maximized" || value == "1")
						finalProps.Mode = WindowMode::Maximized;
					else
						finalProps.Mode = WindowMode::Windowed;
				}
			}
		}

		for (int i = 1; i < args.Count; i++)
		{
			if (strcmp(args[i], "--width") == 0 && i + 1 < args.Count)
			{
				try
				{
					finalProps.Width = std::stoi(args[i + 1]);
				}
				catch (...) {}
			}
			else if (strcmp(args[i], "--height") == 0 && i + 1 < args.Count)
			{
				try
				{
					finalProps.Height = std::stoi(args[i + 1]);
				}
				catch (...) {}
			}
			else if (strcmp(args[i], "--title") == 0 && i + 1 < args.Count)
			{
				finalProps.Title = args[i + 1];
			}
			else if (strcmp(args[i], "--mode") == 0 && i + 1 < args.Count)
			{
				std::string m = args[i + 1];
				if (m == "Fullscreen" || m == "fullscreen" || m == "3")
					finalProps.Mode = WindowMode::Fullscreen;
				else if (m == "Borderless" || m == "borderless" || m == "2")
					finalProps.Mode = WindowMode::Borderless;
				else if (m == "Maximized" || m == "maximized" || m == "1")
					finalProps.Mode = WindowMode::Maximized;
				else
					finalProps.Mode = WindowMode::Windowed;
			}
		}
#endif

		m_Window = std::unique_ptr<Window>(Window::Create(finalProps));
		m_Window->SetEventCallback([this](Event& e) { OnEvent(e); });

		AddSubsystem<JobSystem>();
		AddSubsystem<Renderer>();
		AddSubsystem<Input>(Input::Create());
		AddSubsystem<PhysicsSystem>();
		AddSubsystem<ScriptEngine>();
		AddSubsystem<AssetManager>();
		AddSubsystem<AudioSystem>();
		AddSubsystem<SceneManager>();

		if (enableImGui)
		{
			m_ImGuiLayer = new ImGuiLayer();
			PushOverlay(m_ImGuiLayer);
		}
	}

	Application::~Application()
	{
		RXN_PROFILE_SCOPE();

		for (Layer* layer : m_LayerStack)
		{
			layer->OnDetach();
			delete layer;
		}

		if (m_Subsystems.find(typeid(JobSystem)) != m_Subsystems.end())
			GetSubsystem<JobSystem>()->Shutdown();			
	}

	void Application::PushLayer(Layer* layer)
	{
		RXN_PROFILE_SCOPE();

		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* layer)
	{
		RXN_PROFILE_SCOPE();

		m_LayerStack.PushOverlay(layer);
		layer->OnAttach();
	}

	void Application::OnEvent(Event& e)
	{
		RXN_PROFILE_SCOPE();

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
			Time::Get().OnFrameStart();

			if (!m_Minimized)
			{
				{
					RXN_PROFILE_SCOPE_NAMED("App::SubsystemUpdates");
					for (auto& subsystem : m_SubsystemList)
						subsystem->Update(Time::Get().GetDeltaTime());
				}

				{
					RXN_PROFILE_SCOPE_NAMED("App::FixedUpdates");
					while (Time::Get().ShouldRunFixedUpdate())
					{
						for (Layer* layer : m_LayerStack)
							layer->OnFixedUpdate(Time::Get().GetFixedDeltaTime());
					}
				}

				{
					RXN_PROFILE_SCOPE_NAMED("App::LayerUpdates");
					for (Layer* layer : m_LayerStack)
						layer->OnUpdate(Time::Get().GetDeltaTime());
				}
			}

			if (m_ImGuiLayer)
			{
				RXN_PROFILE_SCOPE_NAMED("App::ImGuiRenderer");
				m_ImGuiLayer->Begin();
				for (Layer* layer : m_LayerStack)
					layer->OnImGuiRenderer();
				m_ImGuiLayer->End();
			}

			{
				RXN_PROFILE_SCOPE_NAMED("App::WindowUpdate");
				m_Window->OnUpdate();
			}

			RXN_PROFILE_FRAME();
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