#include "rxnpch.h"
#include "OpenGLContext.h"

#include <glad/glad.h>

namespace RXNEngine {

	OpenGLContext::OpenGLContext(SDL_Window* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		RXN_CORE_ASSERT(windowHandle, "Window handle is null!");
	}

	void OpenGLContext::Init()
	{
		m_Context = SDL_GL_CreateContext((SDL_Window*)m_WindowHandle);

		RXN_CORE_ASSERT(m_Context, "Failed to create OpenGL context!");

		int status = gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
		RXN_CORE_ASSERT(status, "Failed to initialize Glad!");

		RXN_CORE_INFO("OpenGL Info:");
		RXN_CORE_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
		RXN_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
		RXN_CORE_INFO("  Version: {0}", (const char*)glGetString(GL_VERSION));

		RXN_CORE_ASSERT(GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 5), "RXNEngine requires at least OpenGL version 4.5!");
	}

	void OpenGLContext::SwapBuffers()
	{
		RXN_PROFILE_SCOPE();
		SDL_GL_SwapWindow((SDL_Window*)m_WindowHandle);
	}
}