#pragma once

#include <glm/glm.hpp>
#include <array>
#include "RXNEngine/Core/Math.h"

namespace RXNEngine {

	struct Plane
	{
		glm::vec3 Normal = { 0.f, 1.f, 0.f };
		float Distance = 0.f;

		void Normalize()
		{
			float length = glm::length(Normal);
			Normal /= length;
			Distance /= length;
		}

		float GetSignedDistanceToPlane(const glm::vec3& point) const
		{
			return glm::dot(Normal, point) + Distance;
		}
	};

	class Frustum
	{
	public:
		Frustum() = default;

		void Define(const glm::mat4& viewProjection);

		bool IsBoxVisible(const glm::vec3& min, const glm::vec3& max) const;

	private:
		std::array<Plane, 6> m_Planes;
	};
}