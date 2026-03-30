#pragma once

#include "RXNEngine/Asset/StaticMesh.h"
#include <string>

namespace RXNEngine {

	class ModelSerializer
	{
	public:
		static void Serialize(const std::string& filepath, const Ref<StaticMesh>& mesh);
		static Ref<StaticMesh> Deserialize(const std::string& filepath);
	};

}