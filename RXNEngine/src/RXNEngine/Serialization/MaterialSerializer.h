#pragma once
#include "RXNEngine/Asset/Material.h"
#include <string>

namespace RXNEngine {

    class MaterialSerializer
    {
    public:
        MaterialSerializer(const Ref<Material>& material);

        bool Serialize(const std::string& filepath);
        bool Deserialize(const std::string& filepath);

    private:
        Ref<Material> m_Material;
    };
}