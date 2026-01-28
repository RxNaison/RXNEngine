#include <RXNEngine.h>
#include <RXNEngine/Core/EntryPoint.h>

#include "EditorLayer.h"

namespace RXNEditor {

	class RXNEditor : public Application
	{
	public:
		RXNEditor(const RXNEngine::WindowProps& props) : Application(props)
		{
			PushLayer(new EditorLayer());
		}
	};
}

namespace RXNEngine {
	Application* CreateApplication()
	{
		RXNEngine::WindowProps props;
		props.Title = "RXN Engine Editor";
		props.Width = 1600;
		props.Height = 900;
		props.Mode = RXNEngine::WindowMode::Maximized;

		return new RXNEditor::RXNEditor(props);
	}
}