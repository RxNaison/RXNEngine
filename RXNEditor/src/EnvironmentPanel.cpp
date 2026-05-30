#include "EnvironmentPanel.h"

#include "UI.h"
#include "RXNEngine/Project/Project.h"
#include "RXNEngine/Project/ProjectSerializer.h"
#include "RXNEngine/Audio/AudioSystem.h"
#include "RXNEngine/Core/Application.h"

#include <imgui.h>

namespace RXNEditor {

	EnvironmentPanel::EnvironmentPanel(const RXNEngine::Ref<RXNEngine::SceneRenderer>& context)
	{
		SetContext(context);
	}

	void EnvironmentPanel::SetContext(const RXNEngine::Ref<RXNEngine::SceneRenderer>& context)
	{
		m_Context = context;
	}

	void EnvironmentPanel::OnImGuiRender()
	{
		ImGui::Begin("Settings");
		UI::DrawFloatControl("Exposure", m_Context->GetSettings().Exposure, 0.05f, 0.0f, 100.0f);
		UI::DrawFloatControl("Gamma", m_Context->GetSettings().Gamma, 0.05f, 0.0f, 100.0f);

		UI::DrawFloatControl("Bloom Intensity", m_Context->GetSettings().BloomIntensity, 0.001f, 0.0f, 1.0f, 110.0f);

		UI::DrawCheckbox("Show Colliders", m_Context->GetSettings().ShowColliders);

		float intensity = m_Context->GetScene()->GetSkyboxIntensity();
		if (UI::DrawFloatControl("Skybox Intensity", intensity, 0.05f, 0.0f, 10.0f))
			m_Context->GetScene()->SetSkyboxIntensity(intensity);

		ImGui::Separator();
		ImGui::Text("PCSS Shadow Settings");
		UI::DrawFloatControl("Light Size", m_Context->GetSettings().LightSize, 0.005f, 0.0f, 2.0f);
		UI::DrawFloatControl("Contact Threshold", m_Context->GetSettings().ContactThreshold, 0.05f, 0.0f, 10.0f);
		UI::DrawFloatControl("Contact Sharpness", m_Context->GetSettings().ContactSharpness, 0.05f, 0.1f, 10.0f);
		UI::DrawFloatControl("Sharpening Bias", m_Context->GetSettings().ContactSharpeningBias, 0.01f, 0.0f, 1.0f);

		ImGui::Separator();
		ImGui::Text("Project Settings");

		bool useFMOD = RXNEngine::Project::GetConfig().UseFMOD;

		if (UI::DrawCheckbox("Use FMOD Backend", useFMOD))
		{
			RXNEngine::Project::GetConfig().UseFMOD = useFMOD;

			RXNEngine::ProjectSerializer serializer(RXNEngine::Project::GetActive());
			serializer.Serialize(RXNEngine::Project::GetProjectFilePath());

			RXNEngine::Application::Get().GetSubsystem<RXNEngine::AudioSystem>()->ApplyProjectSettings(useFMOD);
		}

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Toggle between FMOD Studio and Miniaudio backends.");

		ImGui::End();
	}
}
