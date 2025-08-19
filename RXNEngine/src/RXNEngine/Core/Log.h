#pragma once

#include "Base.h"
#include "Assert.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

namespace RXNEngine
{
	class Log
	{
	public:
		static void Init();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}


#define RXN_CORE_TRACE(...)		::RXNEngine::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define RXN_CORE_INFO(...)		::RXNEngine::Log::GetCoreLogger()->info(__VA_ARGS__)
#define RXN_CORE_WARN(...)		::RXNEngine::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define RXN_CORE_ERROR(...)		::RXNEngine::Log::GetCoreLogger()->error(__VA_ARGS__)
#define RXN_CORE_CRITICAL(...)	::RXNEngine::Log::GetCoreLogger()->critical(__VA_ARGS__)

#define RXN_TRACE(...)			::RXNEngine::Log::GetClientLogger()->trace(__VA_ARGS__)
#define RXN_INFO(...)			::RXNEngine::Log::GetClientLogger()->info(__VA_ARGS__)
#define RXN_WARN(...)			::RXNEngine::Log::GetClientLogger()->warn(__VA_ARGS__)
#define RXN_ERROR(...)			::RXNEngine::Log::GetClientLogger()->error(__VA_ARGS__)
#define RXN_CRITICAL(...)		::RXNEngine::Log::GetClientLogger()->critical(__VA_ARGS__)