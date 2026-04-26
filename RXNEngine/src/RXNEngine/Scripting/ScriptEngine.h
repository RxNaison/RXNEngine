#pragma once

#include "RXNEngine/Scene/Scene.h"
#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Core/Base.h"
#include "RXNEngine/Core/Subsystem.h"

#include <string>
#include <vector>

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

	class ScriptEngine;

	class ScriptInstance
	{
	public:
		ScriptInstance(ScriptEngine* engine, Entity entity, const std::string& className);

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

		bool GetFieldValueInternal(const std::string& name, void* outBuffer);
		void SetFieldValueInternal(const std::string& name, const void* inBuffer);
	private:
		ScriptEngine* m_Engine;
		Entity m_Entity;
		std::string m_ClassName;
	};

	struct ScriptEngineData;

	class ScriptEngine : public Subsystem
	{
	public:
		ScriptEngine() = default;
		virtual ~ScriptEngine() = default;

		virtual void Init() override;
		virtual void Update(float deltaTime) override;
		virtual void Shutdown() override;

		void LoadAssembly(const std::string& appFilepath);

		void OnRuntimeStart(Scene* scene);
		void OnRuntimeStop();

		void SetEngineTime(float deltaTime);

		void OnCreateEntity(Entity entity);
		void OnDestroyEntity(Entity entity);

		void OnUpdateEntity(Entity entity, float deltaTime);
		void OnFixedUpdateEntity(Entity entity, float deltaTime);

		void OnCollisionEnter(uint64_t entityID, uint64_t otherID);
		void OnCollisionExit(uint64_t entityID, uint64_t otherID);

		void OnTriggerEnter(uint64_t entityID, uint64_t otherID);
		void OnTriggerExit(uint64_t entityID, uint64_t otherID);

		bool EntityClassExists(const std::string& fullClassName);

		Scene* GetSceneContext();

		void RegisterField(const std::string& className, const std::string& fieldName, ScriptFieldType type);
		const std::vector<ScriptField>& GetClassFields(const std::string& className);

		Ref<ScriptInstance> GetEntityScriptInstance(UUID uuid);

		void ReloadAssembly();
		void ReloadIfModified(float deltaTime);

		ScriptEngineData* GetData() { return m_Data; }

	private:
		ScriptEngineData* m_Data = nullptr;
	};
}