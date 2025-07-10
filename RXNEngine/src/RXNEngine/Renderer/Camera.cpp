#include "rxnpch.h"
#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace RXNEngine {

	Camera::Camera()
	{
		RecalculateViewMatrix();
		RecalculateProjectionMatrix();
	}

	void Camera::SetPerspective(float verticalFOV, float aspectRatio, float nearClip, float farClip)
	{
		m_ProjectionMode = ProjectionMode::Perspective;
		m_PerspectiveFOV = verticalFOV;
		m_AspectRatio = aspectRatio;
		m_PerspectiveNear = nearClip;
		m_PerspectiveFar = farClip;
		RecalculateProjectionMatrix();
	}

	void Camera::SetOrthographic(float size, float aspectRatio, float nearClip, float farClip)
	{
		m_ProjectionMode = ProjectionMode::Orthographic;
		m_OrthographicSize = size;
		m_AspectRatio = aspectRatio;
		m_OrthographicNear = nearClip;
		m_OrthographicFar = farClip;
		RecalculateProjectionMatrix();
	}

	void Camera::RecalculateProjectionMatrix()
	{
		if (m_ProjectionMode == ProjectionMode::Perspective)
		{
			m_ProjectionMatrix = glm::perspective(m_PerspectiveFOV, m_AspectRatio, m_PerspectiveNear, m_PerspectiveFar);
		}
		else
		{
			float orthoLeft = -m_OrthographicSize * m_AspectRatio * 0.5f;
			float orthoRight = m_OrthographicSize * m_AspectRatio * 0.5f;
			float orthoBottom = -m_OrthographicSize * 0.5f;
			float orthoTop = m_OrthographicSize * 0.5f;
			m_ProjectionMatrix = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, m_OrthographicNear, m_OrthographicFar);
		}

		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}

	void Camera::RecalculateViewMatrix()
	{
		glm::mat4 rotationMatrix = glm::toMat4(m_Orientation);
		glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), m_Position);
		m_ViewMatrix = glm::inverse(translationMatrix * rotationMatrix);

		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}
}