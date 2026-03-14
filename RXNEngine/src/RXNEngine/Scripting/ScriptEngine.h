#pragma once

#include "RXNEngine/Scene/Scene.h"

#include <string>

namespace RXNEngine {

	class ScriptEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static void LoadAssembly(const std::string& filepath);

		static void OnRuntimeStart(Scene* scene);
		static void OnRuntimeStop();

		static void OnCreateEntity(Entity entity);
		static void OnUpdateEntity(Entity entity, float ts);

		static bool EntityClassExists(const std::string& fullClassName);

	};
}