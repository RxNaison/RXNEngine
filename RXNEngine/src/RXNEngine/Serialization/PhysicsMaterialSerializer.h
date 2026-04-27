#pragma once
#include "RXNEngine/Asset/PhysicsMaterial.h"
#include <string>

namespace RXNEngine {

    class PhysicsMaterialSerializer
    {
    public:
        PhysicsMaterialSerializer(const Ref<PhysicsMaterial>& material);

        bool Serialize(const std::string& filepath);
        bool Deserialize(const std::string& filepath);

    private:
        Ref<PhysicsMaterial> m_Material;
    };
}