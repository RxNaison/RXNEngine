#pragma once

#ifdef RXN_PLATFORM_WINDOWS

extern RXNEngine::Application* RXNEngine::CreateApplication();

int main(int argc, char** argv)
{
	RXNEngine::Log::Init();
	RXN_CORE_TRACE("Initialized successfull!");
	auto app = RXNEngine::CreateApplication();
	app->Run();
	delete app;
}
#endif