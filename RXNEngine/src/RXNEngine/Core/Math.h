#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace RXNEngine {

    struct AABB {
        glm::vec3 Min;
        glm::vec3 Max;
    };

    namespace Math {

        inline AABB CalculateWorldAABB(const AABB& localAABB, const glm::mat4& transform)
        {
            glm::vec3 localCenter = (localAABB.Min + localAABB.Max) * 0.5f;
            glm::vec3 localExtents = (localAABB.Max - localAABB.Min) * 0.5f;

            glm::vec3 worldCenter = glm::vec3(transform * glm::vec4(localCenter, 1.0f));

            glm::vec3 right = glm::vec3(transform[0]);
            glm::vec3 up = glm::vec3(transform[1]);
            glm::vec3 forward = glm::vec3(transform[2]);

            glm::vec3 worldExtents =
                glm::abs(right) * localExtents.x +
                glm::abs(up) * localExtents.y +
                glm::abs(forward) * localExtents.z;

            return { worldCenter - worldExtents, worldCenter + worldExtents };
        }
    }
}