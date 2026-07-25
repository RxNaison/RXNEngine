#include "rxnpch.h"
#include "RXNEngine/Renderer/Renderer.h"
#include "HiZBuffer.h"
#include "Platform/OpenGL/OpenGLHiZBuffer.h"

namespace RXNEngine {

	Ref<HiZBuffer> HiZBuffer::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    RXN_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLHiZBuffer>();
		}

		RXN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
