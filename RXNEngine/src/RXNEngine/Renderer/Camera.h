#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace RXNEngine {

	class Camera {
	public:
		enum class ProjectionMode
		{
			Perspective, Orthographic
		};

	public:
		Camera();
		virtual ~Camera() = default;

		void SetProjectionMode(ProjectionMode mode) { m_ProjectionMode = mode; RecalculateProjectionMatrix(); }

		// Perspective properties
		void SetPerspective(float verticalFOV, float aspectRatio, float nearClip, float farClip);

		void SetPerspectiveFOV(float verticalFOV) { m_PerspectiveFOV = verticalFOV; RecalculateProjectionMatrix(); }
		void SetPerspectiveNearClip(float nearClip) { m_PerspectiveNear = nearClip; RecalculateProjectionMatrix(); }
		void SetPerspectiveFarClip(float farClip) { m_PerspectiveFar = farClip; RecalculateProjectionMatrix(); }

		// Orthographic properties
		void SetOrthographic(float size, float aspectRatio, float nearClip, float farClip);

		void SetOrthographicSize(float size) { m_OrthographicSize = size; RecalculateProjectionMatrix(); }
		void SetOrthographicNearClip(float nearClip) { m_OrthographicNear = nearClip; RecalculateProjectionMatrix(); }
		void SetOrthographicFarClip(float farClip) { m_OrthographicFar = farClip; RecalculateProjectionMatrix(); }

		//Other
		void SetAspectRatio(float aspectRatio) { m_AspectRatio = aspectRatio; RecalculateProjectionMatrix(); }
		void SetPosition(const glm::vec3& position) { m_Position = position; RecalculateViewMatrix(); }
		void SetOrientation(const glm::quat& orientation) { m_Orientation = orientation; RecalculateViewMatrix(); }

		inline const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		inline const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		inline const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }

		inline glm::vec3 GetForwardDirection() const { return glm::rotate(m_Orientation, glm::vec3(0.0f, 0.0f, -1.0f)); }
		inline glm::vec3 GetUpDirection() const { return glm::rotate(m_Orientation, glm::vec3(0.0f, 1.0f, 0.0f)); }
		inline glm::vec3 GetRightDirection() const { return glm::rotate(m_Orientation, glm::vec3(1.0f, 0.0f, 0.0f)); }

		inline float GetAspectRatio() const { return m_AspectRatio; }

		ProjectionMode GetProjectionMode() const { return m_ProjectionMode; }

	private:
		void RecalculateProjectionMatrix();
		void RecalculateViewMatrix();
	private:

		glm::mat4 m_ViewMatrix{ 1.0f };
		glm::mat4 m_ProjectionMatrix{ 1.0f };
		glm::mat4 m_ViewProjectionMatrix{ 1.0f };

		glm::vec3 m_Position{ 0.0f, 0.0f, 0.0f };
		glm::quat m_Orientation{ 1.0f, 0.0f, 0.0f, 0.0f };

		ProjectionMode m_ProjectionMode = ProjectionMode::Perspective;
		float m_AspectRatio = 1.777778f;

		// Perspective parameters
		float m_PerspectiveFOV = glm::radians(45.0f);
		float m_PerspectiveNear = 0.1f;
		float m_PerspectiveFar = 1000.0f;

		// Orthographic parameters
		float m_OrthographicSize = 10.0f;
		float m_OrthographicNear = -1.0f;
		float m_OrthographicFar = 1.0f;
	};
}