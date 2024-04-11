#pragma once

#include "Core.h"
#include "Events/Event.h"

namespace RXNEngine {

	class RXN_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();
	};

	Application* CreateApplication();

}