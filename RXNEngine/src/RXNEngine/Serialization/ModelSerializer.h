#pragma once

#include "RXNEngine/Asset/StaticMesh.h"
#include "RXNEngine/Asset/SkeletalMesh.h"
#include <string>

namespace RXNEngine {

	class ModelSerializer
	{
	public:
		static void Serialize(const std::string& filepath, const Ref<StaticMesh>& mesh);
		static Ref<StaticMesh> Deserialize(const std::string& filepath);

		static void SerializeSkeletal(const std::string& filepath, const Ref<SkeletalMesh>& mesh);
		static Ref<SkeletalMesh> DeserializeSkeletal(const std::string& filepath);
	};

}