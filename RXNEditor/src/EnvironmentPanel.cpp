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
		ImGui::Begin("Environment");
		ImGui::SliderFloat("Exposure", &m_Context->GetSettings().Exposure, 0, 100);
		ImGui::SliderFloat("Gamma", &m_Context->GetSettings().Gamma, 0, 100);
		ImGui::End();
	}
}
