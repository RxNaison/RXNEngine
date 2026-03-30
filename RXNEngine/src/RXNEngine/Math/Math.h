#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace RXNEngine {

    struct AABB {
        glm::vec3 Min;
        glm::vec3 Max;
    };

	struct Ray
	{
		glm::vec3 Origin;
		glm::vec3 Direction;
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

		inline bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale)
		{
			// From glm::decompose in matrix_decompose.inl

			using namespace glm;

			mat4 LocalMatrix(transform);

			if (epsilonEqual(LocalMatrix[3][3], static_cast<float>(0), epsilon<float>()))
				return false;

			if (
				epsilonNotEqual(LocalMatrix[0][3], static_cast<float>(0), epsilon<float>()) ||
				epsilonNotEqual(LocalMatrix[1][3], static_cast<float>(0), epsilon<float>()) ||
				epsilonNotEqual(LocalMatrix[2][3], static_cast<float>(0), epsilon<float>()))
			{
				LocalMatrix[0][3] = LocalMatrix[1][3] = LocalMatrix[2][3] = static_cast<float>(0);
				LocalMatrix[3][3] = static_cast<float>(1);
			}

			translation = vec3(LocalMatrix[3]);
			LocalMatrix[3] = vec4(0, 0, 0, LocalMatrix[3].w);

			vec3 Row[3], Pdum3;

			for (length_t i = 0; i < 3; ++i)
				for (length_t j = 0; j < 3; ++j)
					Row[i][j] = LocalMatrix[i][j];

			scale.x = length(Row[0]);
			Row[0] = detail::scale(Row[0], static_cast<float>(1));
			scale.y = length(Row[1]);
			Row[1] = detail::scale(Row[1], static_cast<float>(1));
			scale.z = length(Row[2]);
			Row[2] = detail::scale(Row[2], static_cast<float>(1));

			rotation.y = asin(-Row[0][2]);
			if (cos(rotation.y) != 0) {
				rotation.x = atan2(Row[1][2], Row[2][2]);
				rotation.z = atan2(Row[0][1], Row[0][0]);
			}
			else {
				rotation.x = atan2(-Row[2][0], Row[1][1]);
				rotation.z = 0;
			}


			return true;
		}

		inline bool IntersectRayAABB(const Ray& ray, const glm::vec3& min, const glm::vec3& max, float& t)
		{
			float tmin = (min.x - ray.Origin.x) / ray.Direction.x;
			float tmax = (max.x - ray.Origin.x) / ray.Direction.x;

			if (tmin > tmax) std::swap(tmin, tmax);

			float tymin = (min.y - ray.Origin.y) / ray.Direction.y;
			float tymax = (max.y - ray.Origin.y) / ray.Direction.y;

			if (tymin > tymax) std::swap(tymin, tymax);

			if (tmin > tymax || tymin > tmax) return false;

			if (tymin > tmin) tmin = tymin;
			if (tymax < tmax) tmax = tymax;

			float tzmin = (min.z - ray.Origin.z) / ray.Direction.z;
			float tzmax = (max.z - ray.Origin.z) / ray.Direction.z;

			if (tzmin > tzmax) std::swap(tzmin, tzmax);

			if (tmin > tzmax || tzmin > tmax) return false;

			if (tzmin > tmin) tmin = tzmin;
			if (tzmax < tmax) tmax = tzmax;

			t = tmin;
			return true;
		}
    }
}