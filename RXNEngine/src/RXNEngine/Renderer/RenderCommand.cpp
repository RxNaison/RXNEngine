#include "rxnpch.h"
#include "RenderCommand.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace RXNEngine {

	RendererAPI* RenderCommand::s_RendererAPI = new OpenGLRendererAPI;

}