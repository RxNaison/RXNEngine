#pragma once

#include "Core.h"

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


