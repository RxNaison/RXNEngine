#pragma once
#include "RXNEngine/Scene/Entity.h"
#include <string>

namespace RXNEngine {

    class PrefabSerializer
    {
    public:
        static void Serialize(Entity entity, const std::string& filepath);

        static Entity Deserialize(Ref<Scene> targetScene, const std::string& filepath);
    };

}