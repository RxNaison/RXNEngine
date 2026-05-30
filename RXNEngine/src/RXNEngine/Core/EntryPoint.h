#pragma once
#include "Base.h"
#include "Application.h"

#ifdef RXN_PLATFORM_WINDOWS

// Hint to NVIDIA Optimus and AMD PowerXpress to use the dedicated GPU
extern "C" {
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

extern RXNEngine::Application* RXNEngine::CreateApplication(RXNEngine::ApplicationCommandLineArgs args);

int main(int argc, char** argv)
{
	auto app = RXNEngine::CreateApplication({ argc, argv });
	app->Run();
	delete app;
}
#endif