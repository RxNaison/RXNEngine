#include "EnvironmentPanel.h"

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
		ImGui::SliderFloat("Exposure", &m_Context->GetSettings().Exposure, 0, 100);
		ImGui::SliderFloat("Gamma", &m_Context->GetSettings().Gamma, 0, 100);

		ImGui::SliderFloat("Bloom Intensity", &m_Context->GetSettings().BloomIntensity, 0, 1);

		ImGui::Checkbox("Show Colliders", &m_Context->GetSettings().ShowColliders);
		ImGui::End();
	}
}
