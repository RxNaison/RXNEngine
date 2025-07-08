#include "rxnpch.h"
#include "OpenGLContext.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>


namespace RXNEngine {
	OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		RXN_CORE_ASSERT(windowHandle, "Window handle is null!")
	}

	void OpenGLContext::Init()
	{
		glfwMakeContextCurrent(m_WindowHandle);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		RXN_CORE_ASSERT(status, "Failed to initialize Glad!");

		RXN_CORE_INFO("OpenGL Info:");
		RXN_CORE_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
		RXN_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
		RXN_CORE_INFO("  Version: {0}", (const char*)glGetString(GL_VERSION));

		RXN_CORE_ASSERT(GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 5), "RXNEngine requires at least OpenGL version 4.5!");
	}

	void OpenGLContext::SwapBuffers()
	{
		glfwSwapBuffers(m_WindowHandle);
	}
}