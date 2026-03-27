#pragma once

#include "RXNEngine/Renderer/GraphicsContext.h"

#include <SDL3/SDL.h>

struct GLFWwindow;

namespace RXNEngine {

	class OpenGLContext : public GraphicsContext
	{
	public:
		OpenGLContext(SDL_Window* windowHandle);

		virtual void Init() override;
		virtual void SwapBuffers() override;
	private:
		SDL_Window* m_WindowHandle;
		SDL_GLContext m_Context;
	};

}