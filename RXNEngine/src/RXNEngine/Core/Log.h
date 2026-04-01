#pragma once

#include "Base.h"
#include "Assert.h"
#include "Subsystem.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"
#include "Application.h"

namespace RXNEngine
{
	class Log : public Subsystem
	{
	public:
		void Init();

		std::shared_ptr<spdlog::logger>& GetCoreLogger() { return m_CoreLogger; }
		std::shared_ptr<spdlog::logger>& GetClientLogger() { return m_ClientLogger; }
	private:
		std::shared_ptr<spdlog::logger> m_CoreLogger;
		std::shared_ptr<spdlog::logger> m_ClientLogger;
	};
}


#define RXN_CORE_TRACE(...)		Application::Get().GetSubsystem<Log>()->GetCoreLogger()->trace(__VA_ARGS__)
#define RXN_CORE_INFO(...)		Application::Get().GetSubsystem<Log>()->GetCoreLogger()->info(__VA_ARGS__)
#define RXN_CORE_WARN(...)		Application::Get().GetSubsystem<Log>()->GetCoreLogger()->warn(__VA_ARGS__)
#define RXN_CORE_ERROR(...)		Application::Get().GetSubsystem<Log>()->GetCoreLogger()->error(__VA_ARGS__)
#define RXN_CORE_CRITICAL(...)	Application::Get().GetSubsystem<Log>()->GetCoreLogger()->critical(__VA_ARGS__)
												   
#define RXN_TRACE(...)			Application::Get().GetSubsystem<Log>()->GetClientLogger()->trace(__VA_ARGS__)
#define RXN_INFO(...)			Application::Get().GetSubsystem<Log>()->GetClientLogger()->info(__VA_ARGS__)
#define RXN_WARN(...)			Application::Get().GetSubsystem<Log>()->GetClientLogger()->warn(__VA_ARGS__)
#define RXN_ERROR(...)			Application::Get().GetSubsystem<Log>()->GetClientLogger()->error(__VA_ARGS__)
#define RXN_CRITICAL(...)		Application::Get().GetSubsystem<Log>()->GetClientLogger()->critical(__VA_ARGS__)