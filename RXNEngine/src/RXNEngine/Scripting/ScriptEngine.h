#pragma once
#include <string>

namespace RXNEngine {

	class ScriptEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static void LoadAssembly(const std::string& filepath);
	};
}