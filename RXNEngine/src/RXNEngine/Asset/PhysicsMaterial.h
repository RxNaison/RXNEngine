#pragma once
#include "RXNEngine/Core/Base.h"
#include <string>

namespace RXNEngine {

    class PhysicsMaterial
    {
    public:
        PhysicsMaterial() = default;
        PhysicsMaterial(float staticFriction, float dynamicFriction, float restitution)
            : StaticFriction(staticFriction), DynamicFriction(dynamicFriction), Restitution(restitution) {}

        float StaticFriction = 0.5f;
        float DynamicFriction = 0.5f;
        float Restitution = 0.1f;

        inline void SetAssetPath(const std::string& path) { m_AssetPath = path; }
        inline const std::string& GetAssetPath() const { return m_AssetPath; }

    private:
        std::string m_AssetPath;
    };
}