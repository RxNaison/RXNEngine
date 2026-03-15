#pragma once

#include "RXNEngine/Scene/Scene.h"
#include "RXNEngine/Scene/Entity.h"

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
		static void OnUpdateEntity(Entity entity, float deltaTime);

		static bool EntityClassExists(const std::string& fullClassName);

		static Scene* GetSceneContext();

	};

	class ScriptInstance
	{
	public:
		ScriptInstance(Entity entity, const std::string& className);

		void InvokeOnCreate();
		void InvokeOnUpdate(float deltaTime);

	private:
		Entity m_Entity;
		std::string m_ClassName;
	};
}