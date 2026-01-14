#include "rxnpch.h"
#include "RXNEngine/Renderer/Renderer.h"
#include "ShadowMap.h"
#include "Platform/OpenGL/OpenGLShadowMap.h"


namespace RXNEngine {

	Ref<ShadowMap> ShadowMap::Create(uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    RXN_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLShadowMap>();
		}

		RXN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
