#include <RXNEngine.h>

class Sandbox : public RXNEngine::Application
{
public:
	Sandbox()
	{
//		PushOverlay(new RXNEngine::ImGuiLayer());
	}
	~Sandbox()
	{

	}

private:

};


RXNEngine::Application* RXNEngine::CreateApplication() 
{
	return new Sandbox();
}