#pragma once
#include <glm/glm.hpp>

namespace RXNEngine {

    struct PointLight
    {
        glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
        float Intensity = 1.0f;

        glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
        float Radius = 10.0f;

        float Falloff = 1.0f;
        float Padding[3];
    };

    struct DirectionalLight
    {
        glm::vec3 Direction = { -0.5f, -1.0f, -0.5f };
        float Intensity = 1.0f;

        glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
        float Padding = 0.0f;
    };

    struct LightEnvironment
    {
        DirectionalLight DirLight;
        std::vector<PointLight> PointLights;
    };
}