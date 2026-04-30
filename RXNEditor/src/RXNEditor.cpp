#include <RXNEngine.h>
#include <RXNEngine/Core/EntryPoint.h>

#include "EditorLayer.h"

namespace RXNEditor {

	class RXNEditor : public Application
	{
	public:
		RXNEditor(const RXNEngine::WindowProps& props, RXNEngine::ApplicationCommandLineArgs args)
			: Application(props, args) 
		{
			PushLayer(new EditorLayer());
		}
	};
}

namespace RXNEngine {

	Application* CreateApplication(ApplicationCommandLineArgs args)
	{
		RXNEngine::WindowProps props;
		props.Title = "RXNEngine Editor";
		props.Width = 1600;
		props.Height = 900;
		props.Mode = RXNEngine::WindowMode::Maximized;

		return new RXNEditor::RXNEditor(props, args);
	}
}