#pragma once

#include "Camera.h"
#include "RXNEngine/Core/Time.h" 
#include "RXNEngine/Input.h"
#include "RXNEngine/Events/Event.h"

namespace RXNEngine {

	class CameraController
	{
	public:
		enum class ControlMode { Mode2D, Mode3D };

	public:
		CameraController(Camera* camera);

		void OnUpdate(float deltaTime);

		void SetControlMode(ControlMode mode) { m_ControlMode = mode; }
		ControlMode GetControlMode() const { return m_ControlMode; }

		void OnMouseScrolled(float yOffset);

	private:
		void UpdateCameraView();

	private:
		Camera* m_Camera = nullptr;
		ControlMode m_ControlMode = ControlMode::Mode3D;

		glm::vec3 m_Position{ 0.0f, 0.0f, 5.0f };
		float m_MoveSpeed = 5.0f;

		float m_Pitch = 0.0f, m_Yaw = -90.0f;
		float m_MouseSensitivity = 0.1f;
		glm::vec2 m_LastMousePosition{ 0.0f, 0.0f };

		float m_OrthoZoomLevel = 1.0f;
		float m_ZRotation = 0.0f;
		float m_RotationSpeed = 90.0f;
	};

}