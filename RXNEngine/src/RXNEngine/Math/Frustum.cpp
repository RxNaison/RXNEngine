#include "rxnpch.h"
#include "Frustum.h"

namespace RXNEngine {

	void Frustum::Define(const glm::mat4& viewProjection)
	{
		// Gribb-Hartmann Method
		const auto& m = viewProjection;

		// Left Plane:   Row 4 + Row 1
		m_Planes[0].Normal.x = m[0][3] + m[0][0];
		m_Planes[0].Normal.y = m[1][3] + m[1][0];
		m_Planes[0].Normal.z = m[2][3] + m[2][0];
		m_Planes[0].Distance = m[3][3] + m[3][0];

		// Right Plane:  Row 4 - Row 1
		m_Planes[1].Normal.x = m[0][3] - m[0][0];
		m_Planes[1].Normal.y = m[1][3] - m[1][0];
		m_Planes[1].Normal.z = m[2][3] - m[2][0];
		m_Planes[1].Distance = m[3][3] - m[3][0];

		// Bottom Plane: Row 4 + Row 2
		m_Planes[2].Normal.x = m[0][3] + m[0][1];
		m_Planes[2].Normal.y = m[1][3] + m[1][1];
		m_Planes[2].Normal.z = m[2][3] + m[2][1];
		m_Planes[2].Distance = m[3][3] + m[3][1];

		// Top Plane:    Row 4 - Row 2
		m_Planes[3].Normal.x = m[0][3] - m[0][1];
		m_Planes[3].Normal.y = m[1][3] - m[1][1];
		m_Planes[3].Normal.z = m[2][3] - m[2][1];
		m_Planes[3].Distance = m[3][3] - m[3][1];

		// Near Plane:   Row 4 + Row 3
		m_Planes[4].Normal.x = m[0][3] + m[0][2];
		m_Planes[4].Normal.y = m[1][3] + m[1][2];
		m_Planes[4].Normal.z = m[2][3] + m[2][2];
		m_Planes[4].Distance = m[3][3] + m[3][2];

		// Far Plane:    Row 4 - Row 3
		m_Planes[5].Normal.x = m[0][3] - m[0][2];
		m_Planes[5].Normal.y = m[1][3] - m[1][2];
		m_Planes[5].Normal.z = m[2][3] - m[2][2];
		m_Planes[5].Distance = m[3][3] - m[3][2];

		for (auto& plane : m_Planes)
			plane.Normalize();
	}

	bool Frustum::IsBoxVisible(const glm::vec3& min, const glm::vec3& max) const
	{
		for (const auto& plane : m_Planes)
		{
			glm::vec3 positivePoint = min;

			if (plane.Normal.x >= 0) positivePoint.x = max.x;
			if (plane.Normal.y >= 0) positivePoint.y = max.y;
			if (plane.Normal.z >= 0) positivePoint.z = max.z;

			if (plane.GetSignedDistanceToPlane(positivePoint) < 0)
			{
				return false;
			}
		}

		return true;
	}
}