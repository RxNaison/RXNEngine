#pragma once

#include "RXNEngine/Scene/Scene.h"
#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Core/Base.h"

#include <string>

namespace RXNEngine {

	enum class ScriptFieldType
	{
		None = 0,
		Float, Double,
		Bool, Char, Byte, Short, Int, Long,
		UByte, UShort, UInt, ULong,
		Vector2, Vector3, Vector4,
		Entity
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
		void InvokeOnFixedUpdate(float deltaTime);

		template<typename T>
		T GetFieldValue(const std::string& name)
		{
			T value;
			bool success = GetFieldValueInternal(name, &value);
			return value;
		}

		template<typename T>
		void SetFieldValue(const std::string& name, T value)
		{
			SetFieldValueInternal(name, &value);
		}
	private:
		bool GetFieldValueInternal(const std::string& name, void* outBuffer);
		void SetFieldValueInternal(const std::string& name, const void* inBuffer);
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
		static void OnDestroyEntity(Entity entity);

		static void OnUpdateEntity(Entity entity, float deltaTime);
		static void OnFixedUpdateEntity(Entity entity, float deltaTime);

		static void OnCollisionEnter(uint64_t entityID, uint64_t otherID);
		static void OnCollisionExit(uint64_t entityID, uint64_t otherID);

		static void OnTriggerEnter(uint64_t entityID, uint64_t otherID);
		static void OnTriggerExit(uint64_t entityID, uint64_t otherID);

		static bool EntityClassExists(const std::string& fullClassName);

		static Scene* GetSceneContext();

		static void RegisterField(const std::string& className, const std::string& fieldName, ScriptFieldType type);
		static const std::vector<ScriptField>& GetClassFields(const std::string& className);

		static Ref<ScriptInstance> GetEntityScriptInstance(UUID uuid);

		static void ReloadAssembly();
		static void ReloadIfModified(float deltaTime);

	};
}