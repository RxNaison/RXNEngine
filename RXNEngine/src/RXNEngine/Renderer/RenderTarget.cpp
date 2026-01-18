#include "rxnpch.h"
#include "RXNEngine/Renderer/Renderer.h"
#include "RXNEngine/Renderer/RenderTarget.h"
#include "Platform/OpenGL/OpenGLRenderTarget.h"

namespace RXNEngine {

	Ref<RenderTarget> RenderTarget::Create(const RenderTargetSpecification& spec)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    RXN_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLRenderTarget>(spec);
		}

		RXN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}