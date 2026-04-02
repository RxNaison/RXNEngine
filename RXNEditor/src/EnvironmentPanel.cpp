#include "EnvironmentPanel.h"
#include "UI.h"

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
		UI::DrawFloatControl("Exposure", m_Context->GetSettings().Exposure, 0.1, 0.0f, 100.0f);
		UI::DrawFloatControl("Gamma", m_Context->GetSettings().Gamma, 0.1, 0.0f, 100.0f);

		UI::DrawFloatControl("Bloom Intensity", m_Context->GetSettings().BloomIntensity, 0.1f, 0.0f, 100.0f, 110.0f);

		UI::DrawCheckbox("Show Colliders", m_Context->GetSettings().ShowColliders);
		ImGui::End();
	}
}
