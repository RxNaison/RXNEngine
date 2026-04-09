#pragma once

#include "RXNEngine/Scene/Scene.h"

#include <yaml-cpp/yaml.h>

namespace RXNEngine {

	class SceneSerializer
	{
	public:
		SceneSerializer(const Ref<Scene>& scene);

		static void SerializeEntity(YAML::Emitter& out, Entity entity);
		static void DeserializeComponentsToEntity(YAML::Node& entity, Entity deserializedEntity);

		void Serialize(const std::string& filepath);
		void SerializeRuntime(const std::string& filepath);

		bool Deserialize(const std::string& filepath);
		bool DeserializeRuntime(const std::string& filepath);
	private:
		Ref<Scene> m_Scene;
	};

}