#include <RXNEngine.h>
#include <RXNEngine/Core/EntryPoint.h>

#include "EditorLayer.h"

namespace RXNEditor {

	class RXNEditor : public Application
	{
	public:
		RXNEditor() : Application()
		{
			PushLayer(new EditorLayer());
		}
	};
}

namespace RXNEngine {
	Application* CreateApplication()
	{
		return new RXNEditor::RXNEditor();
	}
}