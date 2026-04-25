#pragma once

#include "GraphicsAPI/Texture.h"
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

    struct SpotLight
    {
        glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
        float Intensity = 1.0f;

        glm::vec3 Direction = { 0.0f, -1.0f, 0.0f };
        float Radius = 10.0f;

        glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
        float CutOff = glm::cos(glm::radians(12.5f));

        float Falloff = 1.0f;
        float OuterCutOff = glm::cos(glm::radians(17.5f));
        float Padding[2];

        glm::mat4 LightSpaceMatrix = glm::mat4(1.0f);
        Ref<Texture2D> CookieTexture = nullptr;
        float CookieSize = 1.0f;
    };

    struct LightEnvironment
    {
        DirectionalLight DirLight;
        std::vector<PointLight> PointLights;
        std::vector<SpotLight> SpotLights;

        float EnvironmentIntensity = 1.0f;
    };
}