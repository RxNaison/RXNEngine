#pragma once
#include <RXNEngine.h>

namespace RXNEditor {

	class EnvironmentPanel
	{
    public:
        EnvironmentPanel() = default;
        EnvironmentPanel(const RXNEngine::Ref<RXNEngine::SceneRenderer>& context);

        void SetContext(const RXNEngine::Ref<RXNEngine::SceneRenderer>& context);
        void OnImGuiRender();
    private:
        RXNEngine::Ref<RXNEngine::SceneRenderer> m_Context;
	};

}