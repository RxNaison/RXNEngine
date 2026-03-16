#pragma once

#include "RXNEngine/Scene/Scene.h"
#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Core/Base.h"

#include <string>

namespace RXNEngine {

	enum class ScriptFieldType
	{
		None = 0, Float, Int, Bool, Vector3
	};

	struct ScriptField
	{
		ScriptFieldType Type;
		std::string Name;
	};

	class ScriptInstance
	{
	public:
		ScriptInstance(Entity entity, const std::string& className);

		void InvokeOnCreate();
		void InvokeOnUpdate(float deltaTime);

		float GetFloatField(const std::string& name);
		void SetFloatField(const std::string& name, float value);

	private:
		Entity m_Entity;
		std::string m_ClassName;
	};

	class ScriptEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static void LoadAssembly(const std::string& appFilepath);

		static void OnRuntimeStart(Scene* scene);
		static void OnRuntimeStop();

		static void OnCreateEntity(Entity entity);
		static void OnUpdateEntity(Entity entity, float deltaTime);

		static bool EntityClassExists(const std::string& fullClassName);

		static Scene* GetSceneContext();

		static void RegisterField(const std::string& className, const std::string& fieldName, ScriptFieldType type);
		static const std::vector<ScriptField>& GetClassFields(const std::string& className);

		static Ref<ScriptInstance> GetEntityScriptInstance(UUID uuid);

		static void ReloadAssembly();
		static void ReloadIfModified(float deltaTime);

	};
}