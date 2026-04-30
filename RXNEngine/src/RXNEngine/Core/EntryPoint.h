#pragma once
#include "Base.h"
#include "Application.h"

#ifdef RXN_PLATFORM_WINDOWS

extern RXNEngine::Application* RXNEngine::CreateApplication(RXNEngine::ApplicationCommandLineArgs args);

int main(int argc, char** argv)
{
	auto app = RXNEngine::CreateApplication({ argc, argv });
	app->Run();
	delete app;
}
#endif