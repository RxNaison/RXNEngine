#include "rxnpch.h"
#include "Log.h"

#include "spdlog/sinks/stdout_color_sinks.h"

namespace RXNEngine
{
	void Log::Init()
	{
		spdlog::set_pattern("%^[%T] %n: %v%$");
		m_CoreLogger = spdlog::stdout_color_mt("RXNEngine");
		m_CoreLogger->set_level(spdlog::level::trace);
		
		m_ClientLogger = spdlog::stdout_color_mt("Application");
		m_ClientLogger->set_level(spdlog::level::trace);
	}
}
