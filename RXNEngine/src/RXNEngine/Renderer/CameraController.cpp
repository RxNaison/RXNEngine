#include "rxnpch.h"
#include "CameraController.h"

namespace RXNEngine {

	CameraController::CameraController(Camera* camera)
		: m_Camera(camera)
	{
		SetControlMode(ControlMode::Mode3D);
		UpdateCameraView();
	}

	void CameraController::OnUpdate(float ts)
	{
		if (m_ControlMode == ControlMode::Mode3D)
		{
			float mousePosX = Input::GetMousePosition().first;
			float mousePosY = Input::GetMousePosition().second;

			glm::vec2 mousePos = glm::vec2(mousePosX, mousePosY);
			glm::vec2 delta = (mousePos - m_LastMousePosition) * m_MouseSensitivity;
			m_LastMousePosition = mousePos;

			m_Yaw += delta.x;
			m_Pitch -= delta.y;
			m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);

			glm::vec3 forward = m_Camera->GetForwardDirection();
			glm::vec3 right = m_Camera->GetRightDirection();
			glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

			if (Input::IsKeyPressed('W')) m_Position += forward * m_MoveSpeed * ts;
			if (Input::IsKeyPressed('S')) m_Position -= forward * m_MoveSpeed * ts;
			if (Input::IsKeyPressed('A')) m_Position -= right * m_MoveSpeed * ts;
			if (Input::IsKeyPressed('D')) m_Position += right * m_MoveSpeed * ts;
			if (Input::IsKeyPressed('E')) m_Position += up * m_MoveSpeed * ts; 
			if (Input::IsKeyPressed('Q')) m_Position -= up * m_MoveSpeed * ts; 
		}
		else
		{
			if (Input::IsKeyPressed('W')) m_Position.y += m_MoveSpeed * m_OrthoZoomLevel * ts;
			if (Input::IsKeyPressed('S')) m_Position.y -= m_MoveSpeed * m_OrthoZoomLevel * ts;
			if (Input::IsKeyPressed('A')) m_Position.x -= m_MoveSpeed * m_OrthoZoomLevel * ts;
			if (Input::IsKeyPressed('D')) m_Position.x += m_MoveSpeed * m_OrthoZoomLevel * ts;

			if (Input::IsKeyPressed('Q')) m_ZRotation += m_RotationSpeed * ts;
			if (Input::IsKeyPressed('E')) m_ZRotation -= m_RotationSpeed * ts;
		}

		UpdateCameraView();
	}

	void CameraController::OnMouseScrolled(float yOffset)
	{
		if (m_ControlMode == ControlMode::Mode3D)
		{
			m_MoveSpeed = glm::max(m_MoveSpeed - yOffset, 0.5f);
		}
		else
		{
			m_OrthoZoomLevel = glm::max(m_OrthoZoomLevel - yOffset * 0.1f, 0.25f);
			m_Camera->SetOrthographic(10.0f * m_OrthoZoomLevel, m_Camera->GetAspectRatio(), -1.0f, 1.0f);
		}
	}

	void CameraController::UpdateCameraView()
	{
		glm::quat orientation;
		if (m_ControlMode == ControlMode::Mode3D)
		{
			orientation = glm::quat(glm::vec3(glm::radians(m_Pitch), glm::radians(m_Yaw), 0.0f));
		}
		else
		{
			orientation = glm::quat(glm::vec3(0.0f, 0.0f, glm::radians(m_ZRotation)));
		}
		m_Camera->SetOrientation(orientation);
		m_Camera->SetPosition(m_Position);
	}

}