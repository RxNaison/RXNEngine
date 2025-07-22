#include "rxnpch.h"
#include "VertexArray.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace RXNEngine {

	Ref<VertexArray> VertexArray::Create()
	{
        switch (Renderer::GetAPI())
        {
            case RendererAPI::API::None:    RXN_CORE_ASSERT(false, "RendererAPI::None is not supported!"); return nullptr;
            case RendererAPI::API::OpenGL:  return std::make_shared<OpenGLVertexArray>();
        }

        RXN_CORE_ASSERT(false, "Unknown RendererAPI!");
        return nullptr;
	}

}