#include <RXNEngine.h>
#include <RXNEngine/Core/EntryPoint.h>

#include "EditorLayer.h"

namespace RXNEngine {

	class RXNEditor : public Application
	{
	public:
		RXNEditor() : Application()
		{
			PushLayer(new EditorLayer());
		}
	};

	Application* CreateApplication()
	{
		return new RXNEditor();
	}

}