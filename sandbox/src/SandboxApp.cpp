#include <RXNEngine.h>

class Sandbox : public RXNEngine::Application
{
public:
	Sandbox()
	{

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